#line 1 "voxelizatoin.g.glsl"
layout (triangles) in;
layout (triangle_strip, max_vertices = 9) out;

#define CONSERVATIVE 0
#define FALLBACK 1

#define ASDF 0.7071067812
#define SQRT_2 1.4142135637309

in vec2 v_uv[];
in vec3 v_normal[];

out vec2 uv;
out vec3 normal;
out vec3 f_pos;

flat out int swizz;
flat out vec4 aabb;
flat out vec3 g_l;

uniform int u_resolution;

void swap(inout vec2 a, inout vec2 b) { vec2 t = a; a = b; b = a;}
void swap(inout vec3 a, inout vec3 b) { vec3 t = a; a = b; b = a;}

int SelectDominantAxis(inout vec3 v0, inout vec3 v1, inout vec3 v2,
                       inout vec2 t0, inout vec2 t1, inout vec2 t2)
{   
  vec3 n = cross(v1 - v0, v2 - v0);
  vec3 l = abs(n);
  g_l = l;
  
  //if x is the dominant axis
  if(l.x >= l.y && l.x >= l.z)
  {
    //then we want to rasterize on the YZ plane
    v0 = v0.yzx;
    v1 = v1.yzx;
    v2 = v2.yzx;	
	
    if(n.x > 0)
    {
      vec3 tmp0 = v0; v0 = v1; v1 = tmp0;
      vec2 tmp1 = t0; t0 = t1; t1 = tmp1;
    }
    return 0;
  }
  
  //if y is the dominant axis
  if(l.y >= l.x && l.y >= l.z)
  {
    //then we want to rasterize on the XZ plane
    swizz = 1;
    v0 = v0.xzy;
    v1 = v1.xzy;
    v2 = v2.xzy;
	
    if(n.y < 0)
    {
      vec3 tmp0 = v0; v0 = v1; v1 = tmp0;
      vec2 tmp1 = t0; t0 = t1; t1 = tmp1;
    }
    return 1;
  }
  
  //otherwise rasterize on the XY plane
  swizz = 2;	
  if(n.z > 0)
  {
    vec3 tmp0 = v0; v0 = v1; v1 = tmp0;
    vec2 tmp1 = t0; t0 = t1; t1 = tmp1;
  }
  return 2;
}

//Based off: https://github.com/otaku690/SparseVoxelOctree/blob/master/WIN/SVO/shader/voxelize.geom.glsl
void ExpandTriangle(inout vec3 v0, inout vec3 v1, inout vec3 v2, out vec4 _aabb)
{
 //calculate AABB
 _aabb.x = min(v0.x, min(v1.x, v2.x));
 _aabb.y = min(v0.y, min(v1.y, v2.y));
 _aabb.z = max(v0.x, max(v1.x, v2.x));
 _aabb.w = max(v0.y, max(v1.y, v2.y));
 
 
 float resolutionf = float(u_resolution);
 vec2 hPixel = vec2(1.0f / resolutionf, 1.0f / resolutionf);
 
 //diameter of a pixel
 float pl = SQRT_2 / resolutionf;
 
 _aabb.xy -= hPixel.xy;
 _aabb.zw += hPixel.xy;
 
 vec2 e0 = normalize(vec2(v1.xy - v0.xy));
 vec2 e1 = normalize(vec2(v2.xy - v1.xy));
 vec2 e2 = normalize(vec2(v0.xy - v2.xy));
 
 //TODO optimize to minimal number of instructions
 vec2 n0 = vec2(-e0.y, e0.x);
 vec2 n1 = vec2(-e1.y, e1.x);
 vec2 n2 = vec2(-e2.y, e2.x);
 
 
 v0.xy += pl * ( (e2 / dot(e2, n0)) + (e0 / dot(e0, n2)) );
 v1.xy += pl * ( (e0 / dot(e0, n1)) + (e1 / dot(e1, n0)) );
 v2.xy += pl * ( (e1 / dot(e1, n2)) + (e2 / dot(e2, n1)) );	
}

void main()
{
  vec3 v0 = gl_in[0].gl_Position.xyz;
  vec3 v1 = gl_in[1].gl_Position.xyz;
  vec3 v2 = gl_in[2].gl_Position.xyz;

  vec2 uv0 = v_uv[0];
  vec2 uv1 = v_uv[1];
  vec2 uv2 = v_uv[2];
  
  vec3 n0 = v_normal[0];
  vec3 n1 = v_normal[1];
  vec3 n2 = v_normal[2];
  
#if CONSERVATIVE
  swizz = SelectDominantAxis(v0, v1, v2, uv0, uv1, uv2);
  
#if FALLBACK  
  vec4 _aabb;
  ExpandTriangle(v0, v1, v2, _aabb);
  aabb = _aabb;
#else
  aabb = vec4(-1e34f, -1e34f, 1e34f, 1e34f);
#endif
  gl_Position = vec4(v0, 1); uv = uv0; f_pos = v0; EmitVertex();
  gl_Position = vec4(v1, 1); uv = uv1; f_pos = v1; EmitVertex();
  gl_Position = vec4(v2, 1); uv = uv2; f_pos = v2; EmitVertex();
  EndPrimitive();
#else
  //rasterizing the scene three times like in the voxel cone tracing paper by NVIDIA seems to give
  //way better results than doing a conservative rasterization. It is also faster
  //aabb = vec4(-1e34f, -1e34f, 1e34f, 1e34f);
  
  //swizz = SelectDominantAxis(v0, v1, v2, uv0, uv1, uv2);
  
  swizz = 0;
  gl_Position = vec4(v0.yzx, 1); uv = uv0; normal = n0; f_pos = v0.yzx; EmitVertex();
  gl_Position = vec4(v1.yzx, 1); uv = uv1; normal = n1; f_pos = v1.yzx; EmitVertex();
  gl_Position = vec4(v2.yzx, 1); uv = uv2; normal = n2; f_pos = v2.yzx; EmitVertex();
  EndPrimitive();
  swizz = 1;
  gl_Position = vec4(v0.xzy, 1); uv = uv0; normal = n0; f_pos = v0.xzy; EmitVertex();
  gl_Position = vec4(v1.xzy, 1); uv = uv1; normal = n1; f_pos = v1.xzy; EmitVertex();
  gl_Position = vec4(v2.xzy, 1); uv = uv2; normal = n2; f_pos = v2.xzy; EmitVertex();
  EndPrimitive();
  swizz = 2;
  gl_Position = vec4(v0.xyz, 1); uv = uv0; normal = n0; f_pos = v0.xyz; EmitVertex();
  gl_Position = vec4(v1.xyz, 1); uv = uv1; normal = n1; f_pos = v1.xyz; EmitVertex();
  gl_Position = vec4(v2.xyz, 1); uv = uv2; normal = n2; f_pos = v2.xyz; EmitVertex();
  EndPrimitive();
#endif  
}
