#line 1 "shared.glsl"

#define RESOLUTION 256 //should be a power of two
#define RESOLUTION_MASK (~(RESOLUTION - 1))
#define MAX_DEPTH 8    //must be equal to: log(RESOLUTION) / log(2)
#define VOXEL_SIZE_X (0.33333333f)
#define VOXEL_SIZE_Y (0.33333333f)
#define VOXEL_SIZE_Z (0.33333333f)

#define PI 3.14159265358f

//#define WEIRD_BLENDING

vec4 RGBA8ToVec4(uint val)
{
  return vec4(float((val >> 16) & 0xff), float((val >>  8) & 0xff),
              float((val >>  0) & 0xff), float((val >> 24) & 0xff));
}
uint Vec4ToRGBA8(vec4 val)
{
  return (uint(val.a) & 0xff) << 24 | (uint(val.r) & 0xff) << 16 |
         (uint(val.g) & 0xff) << 8  | (uint(val.b) & 0xff) << 0;
}

ivec3 BrickToImagePos(uint brick, ivec3 size)
{
  ivec3 brickCount = size / 3;
  return ivec3(
     brick %  brickCount.x,
    (brick /  brickCount.x) % brickCount.y,
     brick / (brickCount.x  * brickCount.y)) * 3;
}

vec3 ImagePosToTexPos(ivec3 imagePos, ivec3 size)
{
  return vec3(imagePos) / vec3(size);
}

//Based of Atomic RGBA8Avg from:
//https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-SparseVoxelization.pdf
void AtomicRGBA8Avg( layout ( r32ui ) coherent volatile uimage3D imgUI, uint color, ivec3 pos)
{

  //write 1 to the alpha bit
  uint newVal = (color & 0x00ffffff) | 0x01000000;
  vec4 val = RGBA8ToVec4(newVal);
  
  //we assume zero is stored
  //The actual stored value must equal this variable before we write
  uint prevStoredValue = 0u;
  
  //loop as long as destination gets changed by other threads
  //int i = 0;
  for(;;)
  {
    //DON'T WRITE THESE CONDITIONS IN A WHILE LOOP
    //NVIDIA WILL AGRESSIVLY OPTIMIZE THEM OUT!!!!!
    uint curStoredValue = imageAtomicCompSwap(imgUI, pos, prevStoredValue, newVal);
    if(curStoredValue == prevStoredValue)
      break;
    
    //if(i++ > 10)
    //  break;
  
    //next time we are going to write, this value has to be stored in the buffer, otherwise we have to try again
    prevStoredValue = curStoredValue;
      
    //convert the value to RGBA8
    vec4 rval = RGBA8ToVec4(curStoredValue);
    
    //denormalize the value
    rval.rgb = rval.rgb * rval.a;
    
    //add the value we want to write to 
    vec4 newValF = rval + val;
    
    //normalize the value back
    newValF.rgb /= newValF.a;
    
    //convert the value to RGBA8
    newVal = Vec4ToRGBA8(newValF);
  }
}


struct OctreeNode
{
  uint child;
  uint brick; //could be removed
  uint neighbourXPos;
  uint neighbourXNeg;
  uint neighbourYPos;
  uint neighbourYNeg;
  uint neighbourZPos;
  uint neighbourZNeg;
}; // 32 bytes

//structure to store deferred threads
struct Thread
{
  uint pos;
  uint diffuse;
  uint normal;
  uint emissive;  
}; //16 bytes
