#line 1 "mesh_shader.f.glsl"

in vec2 texcoord;
in vec3 normalWS;
in vec4 posLS; //light space position
in vec3 posVS; //world space position
//in vec3 viewDir;

out vec3 color;


uniform vec3 u_lightDir;
uniform int u_isTextured;
uniform vec4 u_color;
uniform uint u_layer;
uniform uint u_enableGI;
uniform float u_lambda;

uniform layout(binding = 0) sampler2D u_texture;
uniform layout(binding = 1) sampler2D u_depthmap;
uniform layout(binding = 2) sampler3D u_diffuseColorTex;
uniform layout(binding = 3) sampler3D u_normalTex;
uniform layout(binding = 4) sampler3D u_lightTex;

layout(std430, binding = 0) restrict readonly buffer Octree { OctreeNode nodes[]; } octree;


float TestShadow(vec3 lightDirWS, vec3 normalWS, vec4 posLS, out vec2 tc, out vec3 c)
{
  c = vec3(1,1,1);
  vec2 shadowTexcoord = vec2(
    posLS.x / posLS.w * 0.5 + 0.5,
    posLS.y / posLS.w * 0.5 + 0.5);
	float pixelDepth = (posLS.z / posLS.w) * 0.5 + 0.5;
  tc = shadowTexcoord;
  if(clamp(shadowTexcoord.x, 0, 1) == shadowTexcoord.x &&
     clamp(shadowTexcoord.y, 0, 1) == shadowTexcoord.y)
  {
    float bias = 0.005 * tan(acos(dot(-lightDirWS.xyz, normalWS)));
    bias = clamp(bias, 0.0f, 0.01f);
    //float bias = 0.005;
    
    vec2 poissonDisk[4] = vec2[](
      vec2( -0.94201624, -0.39906216 ),
      vec2( 0.94558609, -0.76890725 ),
      vec2( -0.094184101, -0.92938870 ),
      vec2( 0.34495938, 0.29387760 )
      );
    
    float visibility = 1.0f;
    for(int i = 0; i < 4; ++i)
      if(texture(u_depthmap, shadowTexcoord + poissonDisk[i] / 2800.0).r < (pixelDepth - bias))
        visibility -= .25f;
    return visibility;
  }
  return 0.0f;
}


//TODO optimize the hell out of this, this seems like a very big bottleneck :S
vec4 FetchVoxel(vec3 samplePos, int sampleLOD)
{
  vec3 fpos = samplePos;//(samplePos * .5f + vec3(0.5f, 0.5f, 0.5f)) * RESOLUTION;
  ivec3 ipos = ivec3(fpos);
  //check if the sample position is out of bounds
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) != 0)
    return vec4(0);
  
  
  uint nodeIdx = 0u;
  uint layer = clamp(MAX_DEPTH - sampleLOD - 1, 0, MAX_DEPTH - 1);
  //uint layer = MAX_DEPTH - 1;
  for(uint depth = 0; depth < layer; ++depth)
  {
    if(octree.nodes[nodeIdx].child == 0u)
      return vec4(0);
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
      
    //traverse to the child node
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  
  uint brick = octree.nodes[nodeIdx].brick;
  if(brick == 0u)
    return vec4(0);
    
  //ivec3 size = imageSize(u_diffuseColorImage);
  ivec3 size = textureSize(u_diffuseColorTex, 0);
  vec3 sizef = vec3(size);
  ivec3 brickPos = BrickToImagePos(brick, size);
  
  //offset because texel centers will give an unblended corner, instead of when dealing
  // with integers where it's the rounded down value we want.
  vec3 texelPos = vec3(brickPos) + vec3(0.5f);
   
  
  //add the floating point part to interpolate over the voxel cell
  vec3 offset = fpos;
  offset -= vec3(ipos & (0xfffffffe << sampleLOD));
  offset /= float(1 << sampleLOD);
  texelPos += offset;
  
  //normalize the texture coordinate
  texelPos /= vec3(sizef);
 
  return vec4(texture(u_lightTex, texelPos).bgr, texture(u_normalTex, texelPos).a);  
}

//cone angel to cone ratio
//ratio = 2 * tan(angle / 2);

vec4 VCT(vec3 originWS, vec3 dirWS, float coneRatio, float maxDist, vec3 normal)
{
  
  vec3 originVS = (((originWS * .5f) + vec3(.5f)) * float(RESOLUTION)) - vec3(.5f);// + normal * 1.0f;//1.41421356237;
  vec3 dirVS = dirWS;
  
  vec3 offset = normal * 1.1f; //prevent collision with it's own voxel
   
  //vec3 offset = originVS - floor(originVS);
  //offset.x = dirVS.x < 0 ? -offset.x - 0.000001f : 1.0f - offset.x + 0.0000001f;
  //offset.y = dirVS.y < 0 ? -offset.y - 0.000001f : 1.0f - offset.y + 0.0000001f;
  //offset.z = dirVS.z < 0 ? -offset.z - 0.000001f : 1.0f - offset.z + 0.0000001f;
  
  originVS += offset;
  vec4 accum = vec4(0, 0, 0, 0);
  
  
  float minDiameter = 1.0f;
  float dist = minDiameter;
  do
  {
    float coneDiameter = max(minDiameter, coneRatio * dist);
    float lod = clamp(log2(coneDiameter / minDiameter), 0, MAX_DEPTH);
    if(lod > MAX_DEPTH)
      break;
    vec3 conePos = originVS + dirVS * dist;
    
  #if 1
    vec4 value = FetchVoxel(conePos, int(lod));
    
    value.rgb *= 1 << (int(lod) * 3);
    value.rgb /= (1.0f + dist * u_lambda);
  #else
    vec4 sampleValue0 = FetchVoxel(conePos, int(lod + 0));
    vec4 sampleValue1 = FetchVoxel(conePos, int(lod + 1));
    
    float f1 = ceil(lod) - lod;
    float f0 = 1.0f - f1;
    vec4 value = sampleValue0 * f0 + sampleValue1 * f1;
    
    float distWS = (((dist + vec3(.5f)) / RESOLUTION) - vec3(.5f)) * 2.0f
    
    value.a /= (1.0f + u_lambda * dist);
  #endif
    
    float sampleWeight = (1.0f - accum.a);
    accum.rgb += value.rgb * sampleWeight;
    accum.a   += value.a;
    
    dist += coneDiameter;
  } while(dist <= maxDist && accum.a < 1.0);
  
  //NOTE color can be stored divided and multiplied later to trade precision for bigger values
  //accum.rgb *= 2.0;
  
  
  return accum;
}

void main()
{
//1 -> enable material color an shadows. 0 -> material color is 1,1,1.
#if 1
	vec4 materialColor;
	if(u_isTextured != 0)
		materialColor = texture(u_texture, texcoord) * u_color;
	else
		materialColor = u_color;
    
  
  vec2 tc; vec3 c;
  float shadow = TestShadow(u_lightDir, normalWS, posLS, tc, c);
  float diffuse = max(dot(-u_lightDir, normalWS), 0) * shadow;
  float ambient = 0.0;
  
  color = materialColor.xyz * c * (ambient + diffuse);
#else
  color = vec3(1.0f);
#endif

//1 -> enable indirect illumination. 0 -> diable indirect illumination.
#if 1
  if(u_enableGI != 0)
  {
    vec3 c0 = cross(normalWS, vec3(0, 1, 0));
    vec3 c1 = cross(normalWS, vec3(0, 0, 1));
    vec3 tangent;
    if(length(c0) > length(c1))
      tangent = normalize(c0);
    else
      tangent = normalize(c1);
      
    vec3 bitangent = normalize(cross(tangent, normalWS));
    tangent = cross(bitangent, normalWS);
    
    
    //http://simonstechblog.blogspot.nl/2013/01/implementing-voxel-cone-tracing.html
    vec3 directions[6] = 
    {
      vec3( 0.000000f,  1.000000f,  0.000000f),
      vec3( 0.000000f,  0.500000f,  0.866025f),
      vec3( 0.823639f,  0.500000f,  0.267617f),
      vec3( 0.509037f,  0.500000f, -0.700629f),
      vec3(-0.509037f,  0.500000f, -0.700629f),
      vec3(-0.823639f,  0.500000f,  0.267617f),
    };
    

    
    const float weights[6] =
    {
      PI / 4.0f,
      3 * PI / 20.0f,
      3 * PI / 20.0f,
      3 * PI / 20.0f,
      3 * PI / 20.0f,
      3 * PI / 20.0f,    
      
    };
    

    mat3 tangentMatrix = mat3(tangent, normalWS, bitangent);
    
    vec3 ind = vec3(0);
    float ao = 0.0f;
    for(int i = 0; i < 6; ++i)
    {
      vec3 dir = tangentMatrix * directions[i];
      //NOTE: ratio == 2 * tan(angle / 2)
      vec4 result = VCT(posVS, dir, 1.155f, 10000.0f, normalWS);
      //float weight = weights[i];
      float weight = dot(dir, normalWS);
      //float weight = 1.0f;
      
      ao  += (1.0f - result.a) * weight;
      ind += result.rgb * weight;
    }
    color += ind.rgb / 6 * materialColor.rgb;
    //color *= (ao / 6);
  }
#endif


//debugging code to visualize voxels maped to the mesh
#if 0
  if(u_enableGI != 0)
    color = FetchVoxel((posVS * .5f + vec3(0.5f)) * RESOLUTION - vec3(0.5f), int(MAX_DEPTH - u_layer)).aaa;
  else
    color = FetchVoxel((posVS * .5f + vec3(0.5f)) * RESOLUTION - vec3(0.5f), int(MAX_DEPTH - u_layer)).rgb;
#endif
} 
