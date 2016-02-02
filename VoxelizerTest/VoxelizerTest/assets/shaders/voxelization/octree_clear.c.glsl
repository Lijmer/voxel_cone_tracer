#version 430 core
#extension GL_NV_gpu_shader5: enable

layout(local_size_x = 64) in;

uniform layout(location=7) int u_nodeCount;
struct OctreeNode
{
	int childeren[8];
	uint brick;
};
layout(std430, binding=0) buffer Octree { OctreeNode nodes[]; };

void main()
{
	if(gl_GlobalInvocationID.x >= u_nodeCount)
		return;
	
	nodes[gl_GlobalInvocationID.x].color = 0xffff00ff;
	
	barrier();
}
