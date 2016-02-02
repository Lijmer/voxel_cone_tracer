#version 330 core

flat in vec4 v_color;

out vec4 color;

void main()
{
  color = vec4(v_color.rgb, 1);
} 
