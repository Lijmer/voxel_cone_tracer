#line 1 "voxelization/set_neighbour_points.c.glsl"
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(std430, binding=0) restrict buffer Octree { OctreeNode nodes[]; } octree;
uniform layout(location = 0) uint u_layer;

uint Traverse(ivec3 ipos, uint layer)
{
  uint nodeIdx = 0u;
  for(uint depth = 0u; depth < layer; ++depth)
  {
    //if this node has no children, stop traversal
    if(octree.nodes[nodeIdx].child == 0u)
      return 0u;
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1) << 0 ) | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
    
    //traverse to the child node in the direction specified
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  return nodeIdx;
}

void main()
{
  int scale = int(MAX_DEPTH - u_layer);
  ivec3 ipos = ivec3(
    gl_GlobalInvocationID.x << scale,
    gl_GlobalInvocationID.y << scale,
    gl_GlobalInvocationID.z << scale);
  
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) != 0)
    return;
  
  uint nodeIdx = 0u; //start at the root
  for(uint depth = 0u; depth < u_layer; ++depth)
  {
    //if this node has no children, stop traversal
    if(octree.nodes[nodeIdx].child == 0)
      return;
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
      
    //traverse to the child node in the direction specified
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  
  //nothing to blend
  if(octree.nodes[nodeIdx].child == 0)
    return;
    
  uint child = octree.nodes[nodeIdx].child;
  
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
  octree.nodes[child + 1].neighbourZPos = child + 1;
  octree.nodes[child + 2].neighbourZPos = child + 6;
  octree.nodes[child + 3].neighbourZPos = child + 7;
  
  octree.nodes[child + 4].neighbourZNeg = child + 0;    
  octree.nodes[child + 5].neighbourZNeg = child + 5;    
  octree.nodes[child + 6].neighbourZNeg = child + 2;    
  octree.nodes[child + 7].neighbourZNeg = child + 3;

  
  uint parXPos = Traverse(ipos + (ivec3(1, 0, 0) << (MAX_DEPTH - u_layer)), u_layer);
  uint parYPos = Traverse(ipos + (ivec3(0, 1, 0) << (MAX_DEPTH - u_layer)), u_layer);
  uint parZPos = Traverse(ipos + (ivec3(0, 0, 1) << (MAX_DEPTH - u_layer)), u_layer);
  
  if(parXPos != 0u && octree.nodes[parXPos].child != 0u)
  {
    octree.nodes[child + 1].neighbourXPos = octree.nodes[parXPos].child + 0;
    octree.nodes[child + 3].neighbourXPos = octree.nodes[parXPos].child + 2;
    octree.nodes[child + 5].neighbourXPos = octree.nodes[parXPos].child + 4;
    octree.nodes[child + 7].neighbourXPos = octree.nodes[parXPos].child + 6;
  
    octree.nodes[octree.nodes[parXPos].child + 0].neighbourXNeg = child + 1;
    octree.nodes[octree.nodes[parXPos].child + 2].neighbourXNeg = child + 3;
    octree.nodes[octree.nodes[parXPos].child + 4].neighbourXNeg = child + 5;
    octree.nodes[octree.nodes[parXPos].child + 6].neighbourXNeg = child + 7;
  }
  else
  {
    octree.nodes[child + 1].neighbourXPos = 0u;
    octree.nodes[child + 3].neighbourXPos = 0u;
    octree.nodes[child + 5].neighbourXPos = 0u;
    octree.nodes[child + 7].neighbourXPos = 0u;
    //if we are going a partially static tree, we will have to set these pointer to 0
    //otherwise they are already set to 0 by the construction
  }
  
  if(parYPos != 0u && octree.nodes[parYPos].child != 0u)
  {
    octree.nodes[child + 2].neighbourYPos = octree.nodes[parYPos].child + 0;
    octree.nodes[child + 3].neighbourYPos = octree.nodes[parYPos].child + 1;
    octree.nodes[child + 6].neighbourYPos = octree.nodes[parYPos].child + 4;
    octree.nodes[child + 7].neighbourYPos = octree.nodes[parYPos].child + 5;
    
    octree.nodes[octree.nodes[parYPos].child + 0].neighbourYNeg = child + 2;
    octree.nodes[octree.nodes[parYPos].child + 1].neighbourYNeg = child + 3;
    octree.nodes[octree.nodes[parYPos].child + 4].neighbourYNeg = child + 6;
    octree.nodes[octree.nodes[parYPos].child + 5].neighbourYNeg = child + 7;
  }
  else
  {
    octree.nodes[child + 2].neighbourYPos = 0u;
    octree.nodes[child + 3].neighbourYPos = 0u;
    octree.nodes[child + 6].neighbourYPos = 0u;
    octree.nodes[child + 7].neighbourYPos = 0u;
    //if we are going a partially static tree, we will have to set these pointer to 0
    //otherwise they are already set to 0 by the construction
  }
  
  if(parZPos != 0u && octree.nodes[parZPos].child != 0u)
  {
    octree.nodes[child + 4].neighbourZPos = octree.nodes[parZPos].child + 0;
    octree.nodes[child + 5].neighbourZPos = octree.nodes[parZPos].child + 1;
    octree.nodes[child + 6].neighbourZPos = octree.nodes[parZPos].child + 2;
    octree.nodes[child + 7].neighbourZPos = octree.nodes[parZPos].child + 3;
    
    octree.nodes[octree.nodes[parYPos].child + 0].neighbourZNeg = child + 4;
    octree.nodes[octree.nodes[parYPos].child + 1].neighbourZNeg = child + 5;
    octree.nodes[octree.nodes[parYPos].child + 2].neighbourZNeg = child + 6;
    octree.nodes[octree.nodes[parYPos].child + 3].neighbourZNeg = child + 7;
  }
  else
  {
    octree.nodes[child + 4].neighbourZPos = 0u;
    octree.nodes[child + 5].neighbourZPos = 0u;
    octree.nodes[child + 6].neighbourZPos = 0u;
    octree.nodes[child + 7].neighbourZPos = 0u;
    //if we are going a partially static tree, we will have to set these pointer to 0
    //otherwise they are already set to 0 by the construction
  }
}
