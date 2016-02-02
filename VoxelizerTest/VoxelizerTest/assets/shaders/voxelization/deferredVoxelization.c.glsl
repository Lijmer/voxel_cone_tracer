#line 1 "deferredVoxelization.c.glsl"

layout(local_size_x = 64) in;




uniform layout(r32ui, binding = 0) coherent volatile uimage3D u_diffuseColorImage;
uniform layout(r32ui, binding = 1) coherent volatile uimage3D u_normalImage;

layout(std430, binding = 0) restrict buffer Octree { OctreeNode nodes[];   } octree;
layout(std430, binding = 1) restrict buffer Locks  { volatile int locks[]; } locks;
layout(std430, binding = 2) restrict buffer A      { Thread threads[];     } threadListIn;
layout(std430, binding = 3) restrict buffer B      { Thread threads[];     } threadListOut;
uniform layout(binding = 4, offset = 0) atomic_uint u_nodeCounter;
uniform layout(binding = 4, offset = 4) atomic_uint u_brickCounter;
layout(std430, binding = 5) restrict buffer C
{
  uint groupCountX;
  uint groupCountY;
  uint groupCountZ;
  uint threadCountX0;
  uint threadCountX1;
  uint lastGroupCalled;
} computeParamsIn;
layout(std430, binding = 6) restrict buffer D
{
  uint groupCountX;
  uint groupCountY;
  uint groupCountZ;
  uint threadCountX0;
  uint threadCountX1;
  uint lastGroupCalled;
} computeParamsOut;

uniform uint u_limit;

void StoreThread(ivec3 pos, uint color, uint normal, vec3 emissive)
{
  //store thread for later execution
  //uint threadIdx = atomicCounterIncrement(u_threadCounter);
  //  atomicMax(computeParamsOut.groupCountX, (threadIdx + 1 + 255) / 256);
  //atomicMax(computeParamsOut.threadCountX0, threadIdx + 1);
  
  uint threadIdx = atomicAdd(computeParamsOut.threadCountX0, 1);
  atomicMax(computeParamsOut.groupCountX, (threadIdx + 1 + 63) / 64);
  
  threadListOut.threads[threadIdx].pos      = (pos.x & 0x3ff) | ((pos.y & 0x3ff) << 10) | ((pos.z & 0x3ff) << 20);
  threadListOut.threads[threadIdx].diffuse  = color;
  threadListOut.threads[threadIdx].normal   = normal; //TODO calculate normal
  //threadListOut.threads[threadIdx].emissive = 0xaaaa5555; //TODO calculate emissive
}

void AllocateBrick(uint idx)
{
  octree.nodes[idx].brick = idx;;atomicCounterIncrement(u_brickCounter);
}

void AllocateChilderen(uint idx, uint depth)
{
  //allocate 8 new nodes
  uint oldVal = atomicCounterIncrement(u_nodeCounter);
  uint childIdx = (oldVal - 1) * 8 + 1;

  if(depth < MAX_DEPTH - 1)
  {
    uint child = childIdx;
    //Internal X neighbours
    octree.nodes[child + 0].neighbourXPos = child + 1;
    octree.nodes[child + 2].neighbourXPos = child + 3;
    octree.nodes[child + 4].neighbourXPos = child + 5;
    octree.nodes[child + 6].neighbourXPos = child + 7;
    
    octree.nodes[child + 1].neighbourXNeg = child + 0;    
    octree.nodes[child + 3].neighbourXNeg = child + 2;    
    octree.nodes[child + 5].neighbourXNeg = child + 4;    
    octree.nodes[child + 7].neighbourXNeg = child + 6;
  
    
    //internal Y neighbours
    octree.nodes[child + 0].neighbourYPos = child + 2;
    octree.nodes[child + 1].neighbourYPos = child + 3;
    octree.nodes[child + 4].neighbourYPos = child + 6;
    octree.nodes[child + 5].neighbourYPos = child + 7;
    
    octree.nodes[child + 2].neighbourYNeg = child + 0;
    octree.nodes[child + 3].neighbourYNeg = child + 1;
    octree.nodes[child + 6].neighbourYNeg = child + 4;    
    octree.nodes[child + 7].neighbourYPos = child + 5;
  
    
    //internal Z neighbours
    octree.nodes[child + 0].neighbourZPos = child + 4;
    octree.nodes[child + 1].neighbourZPos = child + 5;
    octree.nodes[child + 2].neighbourZPos = child + 6;
    octree.nodes[child + 3].neighbourZPos = child + 7;
    
    octree.nodes[child + 4].neighbourZNeg = child + 0;    
    octree.nodes[child + 5].neighbourZNeg = child + 1;    
    octree.nodes[child + 6].neighbourZNeg = child + 2;    
    octree.nodes[child + 7].neighbourZNeg = child + 3;
  
    //optionally initialize the childeren here
    for(int i = 0; i < 8; ++i)
      AllocateBrick(childIdx + i);
  }
  
  //assign pointer to 8 new childeren
  octree.nodes[idx].child = childIdx;
}


void main()
{
  //if(gl_GlobalInvocationID.x >= u_limit)
  //  return;

  if(gl_GlobalInvocationID.x == 0)
  {
    atomicExchange(computeParamsIn.threadCountX1, computeParamsIn.threadCountX0);
    atomicExchange(computeParamsIn.threadCountX0, 0);
    atomicExchange(computeParamsOut.threadCountX1, 0);
    
    uint lastGroupID = computeParamsIn.lastGroupCalled;
    atomicExchange(computeParamsOut.lastGroupCalled, lastGroupID + 1);
  }
  
  //don't execute threads that are out of bounds
  uint limit = max(computeParamsIn.threadCountX0, computeParamsIn.threadCountX1);
  if(gl_GlobalInvocationID.x >= limit)
    return;
   
  uint in_pos      = threadListIn.threads[gl_GlobalInvocationID.x].pos;
  uint in_diffuse  = threadListIn.threads[gl_GlobalInvocationID.x].diffuse;
  uint in_normal   = threadListIn.threads[gl_GlobalInvocationID.x].normal;
  //uint in_emissive = threadListIn.threads[gl_GlobalInvocationID.x].emissive;
  
  ivec3 ipos = ivec3(
    in_pos & 0x3ff,
    (in_pos >> 10) & 0x3ff,
    (in_pos >> 20) & 0x3ff
  );
  
  
  //state variables for tree traversal
  uint nodeIdx = 0; //0 is root node
  
  //keep going deeper until we reached the bottom
  for(uint depth = 0u; depth < MAX_DEPTH - 1; ++depth)
  {
    //check if the node doesn't have childeren allocated
    if(octree.nodes[nodeIdx].child == 0)
    {
      //if that's the case we want to create 8 new childeren
    
      //first we need to lock the node down
      bool locked = atomicExchange(locks.locks[nodeIdx], 1) == 0;
    
      //if we managed to aquire the lock, we can go ahead and continue
      if(locked)
      {
        //check if not another thread created childeren while we were aquiring the lock
        if(octree.nodes[nodeIdx].child == 0)
          AllocateChilderen(nodeIdx, depth); //it is now safe to allocate new childeren
        
        //memoryBarrier here, because we first write 1, and then 0. Compiler expects it can optimize by just writing 0
        //However, we are in a multi threaded environment, so we need to indicate that the compiler can't argue about this
        memoryBarrier();
        
        //unlock the node
        atomicExchange(locks.locks[nodeIdx], 0);
      }
      else //otherwise we will store this thread for later execution to avoid active waiting
      {
        //store the thread
        StoreThread(ipos, in_diffuse, in_normal, vec3(0,0,0));
          
        //I am not sure whether to run discard or return, discard seems to make more sense to me
        //discard; //stop running this shader
        return; //stop running this shader
      }
    }
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
      
    //traverse to the child node
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
   
  uint color = in_diffuse;
  
  //if(octree.nodes[nodeIdx].brick == 0u)
  //{
  //  bool locked = atomicExchange(locks.locks[nodeIdx], 1) == 0;    
  //  if(locked)
  //  {
  //    if(octree.nodes[nodeIdx].child == 0)
  //      AllocateBrick(nodeIdx);        
  //    memoryBarrier();      
  //    atomicExchange(locks.locks[nodeIdx], 0);
  //  }
  //  else
  //  {
  //    StoreThread(ipos, color, vec3(0, 0, 0), vec3(0, 0, 0));
  //    return;
  //  }
  //}  
  
  uint brick = octree.nodes[nodeIdx].brick;

  AtomicRGBA8Avg(u_diffuseColorImage, in_diffuse, BrickToImagePos(brick, imageSize(u_diffuseColorImage)) + (ipos & 0x1));
  AtomicRGBA8Avg(u_normalImage, in_normal, BrickToImagePos(brick, imageSize(u_normalImage)) + (ipos & 0x1));

}
