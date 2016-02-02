#version 330 core

in layout(location=0) vec3 in_position;
in layout(location=1) vec3 in_normal;
in layout(location=2) vec3 in_offset;
in layout(location=3) vec4 in_color;

flat out vec4 v_color;

uniform mat4 u_modelMatrix;
uniform mat4 u_worldMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
//uniform mat4 u_wvpMatrix;

void main()
{
  vec4 position = vec4(0,0,0,0);
  position += u_modelMatrix * vec4(in_position, 1);
  position += u_worldMatrix * vec4(in_offset,   1);
  
  v_color = in_color.xyzw;
  gl_Position = (u_projectionMatrix * u_viewMatrix) * position;
} 
