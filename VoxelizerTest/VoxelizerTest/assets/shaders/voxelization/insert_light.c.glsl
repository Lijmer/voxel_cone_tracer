#line 2 "insert_light.c.glsl"

layout(local_size_x = 32, local_size_y = 32) in;

uniform layout(r32ui, binding = 0) coherent volatile uimage3D u_diffuseColorImage;
uniform layout(r32ui, binding = 1) coherent volatile uimage3D u_normalImage;
uniform layout(r32ui, binding = 2) coherent volatile uimage3D u_lightImage;


layout(std430, binding = 0) buffer Octree { OctreeNode nodes[]; } octree;

uniform mat4 u_invMatrix;
uniform mat4 u_worldSpaceToVoxelSpaceMatrix;
uniform vec3 u_lightVector;

uniform sampler2D u_depthmap;

vec3 Unpack(uint i)
{
  return vec3(
    ((i >>  0) & 0xff),
    ((i >>  8) & 0xff),
    ((i >>  16) & 0xff));
}

uint Pack(vec3 v)
{
  return (uint(v.x) & 0xff) << 0 |
         (uint(v.y) & 0xff) << 8 |
         (uint(v.z) & 0xff) << 16;
}

void Traverse(ivec3 ipos)
{
  uint nodeIdx = 0; //set current node to root
  
  //traverse down until we reach the bottom
  for(uint depth = 0u; depth < MAX_DEPTH - 1; ++depth)
  {
    //empty node. TODO: figure out what to do with them
    if(octree.nodes[nodeIdx].child == 0)
      return;
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
     
    //traverse to child node
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  
  if(octree.nodes[nodeIdx].brick != 0)
  {
    ////write magenta to them for now
    //atomicExchange(octree.nodes[nodeIdx].brick, 0xff00ff);  .
    
    uint brick = octree.nodes[nodeIdx].brick;
    
    
    ivec3 imagePos = BrickToImagePos(brick, imageSize(u_lightImage)) + (ipos & 1);
    
    //vec4 lightColor = uvec4(1, 0, 1, 1);
    //vec4 albedo = vec4(imageLoad(u_diffuseColorImage, imagePos)) / 255.0f;
    //albedo.a = 1.0f;
    //
    //uvec4 color = uvec4((lightColor * albedo) * 255.0f);
    
    uint packedColor = imageLoad(u_diffuseColorImage, imagePos).r;
    
    if(packedColor != 0)
    {
      uint packedNormal = imageLoad(u_normalImage, imagePos).r;
      
      vec3 color = Unpack(packedColor);
      vec3 normal = normalize(Unpack(packedNormal) - 127.5f);
      
      //store pre filtered diffuse light
      color *= dot(-u_lightVector, normal);
      
      packedColor = Pack(color) | 0xff000000;
        
      
      imageStore(u_lightImage, imagePos, uvec4(packedColor, 0, 0, 0));
    
    
      //AtomicRGBA8Avg(u_diffuseColorImage, nodeIdx, ucolor, BrickToImagePos(brick, imageSize(u_diffuseColorImage)) + (ipos & 0x1));
    }
  }
}

void main()
{
  ivec2 texSize = textureSize(u_depthmap, 0);
  vec2 uv = vec2(
    float(gl_GlobalInvocationID.x) / float(texSize.x),
    float(gl_GlobalInvocationID.y) / float(texSize.y)
  );
  
  vec4 depthVal = texture(u_depthmap, uv);
  vec4 texPos = vec4(uv * 2 - vec2(1, 1), depthVal.r * 2.0f - 1.0f, 1.0f);
  vec4 worldPos = u_invMatrix * texPos;	
  vec4 voxelPos =  u_worldSpaceToVoxelSpaceMatrix * worldPos * vec4(128, 128, 128, 1) + vec4(128, 128, 128, 0);
  vec3 pos = voxelPos.xyz;
  
  //state variables
  ivec3 ipos = ivec3(pos);	
  //check if the node is out of bounds
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) != 0)
    return;
  
  Traverse(ipos);
}
