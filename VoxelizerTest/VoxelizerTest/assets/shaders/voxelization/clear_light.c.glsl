#line 1 clear_light.c.glsl

uniform layout(rgba8ui, binding = 2) restrict uimage3D u_lightImage;
layout(std430, binding = 0) restrict buffer Octree
{
  OctreeNode nodes[];
}octree;

void main()
{
  
  
}
