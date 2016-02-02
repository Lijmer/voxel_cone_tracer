#line 1 "voxelization.f.glsl"
out vec3 color;

flat in int swizz;
flat in vec4 aabb;
flat in vec3 g_l;
in vec2 uv;
in vec3 normal;
in vec3 f_pos;

uniform int u_isTextured;
uniform sampler2D u_texture;
uniform vec4 u_color;
uniform int u_resolution;

out vec4 out_color;

uniform layout(r32ui, binding = 0) coherent volatile uimage3D u_diffuseColorImage;
uniform layout(r32ui, binding = 1) coherent volatile uimage3D u_normalImage;

layout(std430, binding = 0) restrict buffer Octree { OctreeNode nodes[];   } octree;
layout(std430, binding = 1) restrict volatile buffer Locks  { volatile int locks[]; } locks;
layout(std430, binding = 3) restrict buffer A      { Thread threads[];     } threadListOut;
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
} computeParamsOut;

void StoreThread(ivec3 pos, uint color, uint norm, vec3 emissive)
{
  //store thread for later execution
  //uint threadIdx = atomicCounterIncrement(u_threadCounter);
  //  atomicMax(computeParamsOut.groupCountX, (threadIdx + 1 + 255) / 256);
  //atomicMax(computeParamsOut.threadCountX0, threadIdx + 1);
  
  uint threadIdx = atomicAdd(computeParamsOut.threadCountX0, 1);
  atomicMax(computeParamsOut.groupCountX, (threadIdx + 1 + 63) / 64);
  

  
  threadListOut.threads[threadIdx].pos      = (pos.x & 0x3ff) | ((pos.y & 0x3ff) << 10) | ((pos.z & 0x3ff) << 20);
  threadListOut.threads[threadIdx].diffuse  = color;
  threadListOut.threads[threadIdx].normal   = norm;
  //threadListOut.threads[threadIdx].emissive = 0xaaaa5555; //TODO calculate emissive
}

void AllocateBrick(uint idx)
{
  octree.nodes[idx].brick = idx;//atomicCounterIncrement(u_brickCounter);
  

  
  //ivec3 brickPos = BrickToImagePos(idx, imageSize(u_diffuseColorImage));
  //for(int i = 0; i < 8; ++i)
  //{
  //  ivec3 offset = ivec3(i, i >> 1, i >> 2);
  //  imageStore(u_diffuseColorImage, brickPos + offset, uvec4(0xff00ff00, 0, 0, 0));
  //}
  
  
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


void TraverseAndFill(ivec3 ipos, uint color)
{
  vec3 n = normalize(normal);
  uint packedNormal =
    (uint((n.x * .5f + .5f) * 255) << 0) |
    (uint((n.y * .5f + .5f) * 255) << 8) |
    (uint((n.z * .5f + .5f) * 255) << 16);

  //current node
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
      
        //BARI BARI NO PISTOL!!!! LUFFY-SENPAI!!!!
        memoryBarrier();
                
        //unlock the node
        atomicExchange(locks.locks[nodeIdx], 0);
      }
      else //otherwise we will store this thread for later execution to avoid active waiting
      {
        //store the thread
        out_color = vec4(0,0,1,1);
        StoreThread(ipos, color, packedNormal, vec3(0,0,0));
        return;
      }
    }
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
    
    //traverse to the child node in the direction specified
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  
  //if(octree.nodes[nodeIdx].brick == 0u)
  //{
  //  memoryBarrier();
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
  //    out_color = vec4(1,0,0,0);
  //    StoreThread(ipos, color, vec3(0, 0, 0), vec3(0, 0, 0));
  //    return;
  //  }
  //}  

  uint brick = octree.nodes[nodeIdx].brick;
  
  AtomicRGBA8Avg(u_diffuseColorImage, color, BrickToImagePos(brick, imageSize(u_diffuseColorImage)) + (ipos & 0x1));
  AtomicRGBA8Avg(u_normalImage, packedNormal, BrickToImagePos(brick, imageSize(u_normalImage)) + (ipos & 0x1));
}

void main()
{
  //calculate the fragment position
  vec3 pos = vec3(gl_FragCoord.xy, gl_FragCoord.z * u_resolution);

       if(swizz == 0) pos = pos.zxy;
  else if(swizz == 1) pos = pos.xzy;
  //else if(swizz == 2) {}//no need for swizeling //pos = pos.xyz;

  //if(f_pos.x < aabb.x || f_pos.x > aabb.z || f_pos.y < aabb.y || f_pos.y > aabb.w)
  //  discard;
  
  
  //convert the float position to an integer position
  ivec3 ipos = ivec3(pos.x, pos.y, pos.z);
  
  //check if the node is out of bounds
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) != 0)
  {
    //if so, discard this fragment
    discard;
  }
  
  //get the material color and color from the texture
  vec4 color = u_color;
  if(u_isTextured != 0)
    color *= texture(u_texture, uv);
  
  uint ucolor = Vec4ToRGBA8(color * 255.0f);
  
  out_color = vec4(0,0,0,0);
  TraverseAndFill(ipos, ucolor);

  
  
  //uint brick = octree.nodes[nodeIdx].brick;  
  //AtomicRGBA8Avg(u_diffuseColorImage, ucolor, BrickToImagePos(brick, imageSize(u_diffuseColorImage)) + (ipos & 0x1));
}
