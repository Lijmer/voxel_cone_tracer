#line 1 "mesh_shader.v.glsl"

in layout(location=0) vec3 in_position;
in layout(location=1) vec3 in_normal;
in layout(location=2) vec2 in_texcoord;

out vec2 texcoord;
out vec3 normalWS;
out vec4 posLS; //light space position
out vec3 posVS;
//out vec3 viewDir;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform mat4 u_wvpMatrix;
uniform mat4 u_lightMatrix;
uniform mat4 u_voxelMatrix;

void main()
{
  vec4 positionMS = vec4(in_position, 1);
  texcoord = in_texcoord;
  normalWS = in_normal;
  
  vec4 positionWS = u_worldMatrix * positionMS;
  posLS = u_lightMatrix * positionWS;
  posVS = (u_voxelMatrix * positionWS).xyz;
  
  //viewDir = -normalize(u_viewMatrix * vec4(positionWS)).xyz;
  
  #if 0
  gl_Position = u_lightMatrix * positionWS; 
  #else
  gl_Position = u_wvpMatrix * positionMS;
  #endif
}