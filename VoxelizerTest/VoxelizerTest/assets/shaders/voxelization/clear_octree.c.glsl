#line 1 "clear_octree.glsl"
layout(local_size_x = 64) in;

uniform uint u_limit;

uniform layout(rgba8ui, binding = 0) uimage3D u_diffuseColorImage;
uniform layout(rgba8ui, binding = 1) uimage3D u_normalImage;
uniform layout(rgba8ui, binding = 2) uimage3D u_lightImage;

layout(std430, binding = 0) buffer Octree
{
  OctreeNode nodes[];
}octree;

void main()
{ 
  if(gl_GlobalInvocationID.x < u_limit)
  {
    octree.nodes[gl_GlobalInvocationID.x].child = 0;
    
    uint brick = octree.nodes[gl_GlobalInvocationID.x].brick;
    if(brick != 0u)
    {
      //TODO clear the texture brick to black
      ivec3 brickPos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
      for(int i = 0; i < 3*3*3; ++i)
      {
        ivec3 offset = ivec3(i, i >> 2, i >> 4) & 0x3;
        ivec3 texelPos = brickPos + offset;
        imageStore(u_diffuseColorImage, texelPos, uvec4(0,0,0,0));
        imageStore(u_normalImage, texelPos, uvec4(0,0,0,0));
        imageStore(u_lightImage, texelPos, uvec4(0,0,0,0));
      }
      
      octree.nodes[gl_GlobalInvocationID.x].brick = 0;
      
      octree.nodes[gl_GlobalInvocationID.x].neighbourXPos = 0u;
      octree.nodes[gl_GlobalInvocationID.x].neighbourXNeg = 0u;
      octree.nodes[gl_GlobalInvocationID.x].neighbourYPos = 0u;
      octree.nodes[gl_GlobalInvocationID.x].neighbourYNeg = 0u;
      octree.nodes[gl_GlobalInvocationID.x].neighbourZPos = 0u;
      octree.nodes[gl_GlobalInvocationID.x].neighbourZNeg = 0u;
    }
    
  }
}
