#line 1 "voxelization.v.glsl"
in layout(location = 0) vec3 in_position;
in layout(location = 1) vec3 in_normal;
in layout(location = 2) vec2 in_uv;

uniform mat4 u_worldMatrix;

out vec2 v_uv;
out vec3 v_normal;

void main()
{
  v_uv = in_uv;
  v_normal = vec3(u_worldMatrix * vec4(in_normal, 0));
  gl_Position = u_worldMatrix * vec4(in_position, 1);
}
