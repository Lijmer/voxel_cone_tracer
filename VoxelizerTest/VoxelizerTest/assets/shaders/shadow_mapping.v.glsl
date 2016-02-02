#version 330 core

in layout(location=0) vec3 in_position;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjMatrix;

void main()
{
	gl_Position = (u_viewProjMatrix * u_worldMatrix) * vec4(in_position, 1.0f);
}
