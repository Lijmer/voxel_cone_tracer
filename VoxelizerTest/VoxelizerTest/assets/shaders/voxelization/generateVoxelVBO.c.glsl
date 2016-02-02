#line 1 "generateVoxelVBO.c.glsl"

layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

uniform layout(rgba8ui, binding = 0) uimage3D u_diffuseColorImage;

layout(std430, binding = 0) restrict buffer Octree { OctreeNode nodes[]; } octree;
layout(packed, binding = 1) restrict buffer A { float positions[]; } positions;
layout(std430, binding = 2) restrict buffer B { uint colors[]; } colors;
uniform layout(binding = 3) atomic_uint counter;
uniform layout(location = 0) uint u_layer;

void main()
{
  uint scale = MAX_DEPTH - u_layer + 1;
  ivec3 ipos = ivec3(
    gl_GlobalInvocationID.x << scale,
    gl_GlobalInvocationID.y << scale,
    gl_GlobalInvocationID.z << scale);
  
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) != 0)
    return;
      
  uint nodeIdx = 0u;
  for(uint depth = 0u; depth < u_layer - 1; ++depth)
  {
    //traverse to the child node in the direction specified
    if(octree.nodes[nodeIdx].child == 0u)
      return;
  
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
    
    //traverse to child node
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  
  //ivec3 child3d = ipos & 0x1;
  //int child = child3d.x | (child3d.y << 1) | (child3d.z << 2);  
  //nodeIdx = octree.nodes[nodeIdx].child;
  
  uint brick = octree.nodes[nodeIdx].brick;
  if(brick == 0u)
    return;
  
  ivec3 brickPos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));

  for(int i = 0; i < 8; ++i)
  {
    ivec3 texelPos = brickPos + (ivec3(i, i>>1, i>>2) & 1);
    uvec4 c = imageLoad(u_diffuseColorImage, texelPos);
    if(c.a == 0)
      continue;
    uint color = (c.r << 16) | (c.g << 8) | (c.b << 0) | (c.a << 24);
    
    uint idx = atomicCounterIncrement(counter);
    colors.colors[idx] = color;
  
    
  
    vec3 pos = vec3(ipos + ((ivec3(i, i>>1, i>>2) & 1) << (scale - 1)));
    pos += -vec3(RESOLUTION / 2, RESOLUTION / 2, RESOLUTION / 2) - vec3(.5, .5, .5);
    int offset = 256 >> (u_layer);
    pos += vec3(offset, offset, offset);
    pos *= vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z);
    
    positions.positions[idx * 3 + 0] = pos.x;
    positions.positions[idx * 3 + 1] = pos.y;
    positions.positions[idx * 3 + 2] = pos.z;
    
  }
}
