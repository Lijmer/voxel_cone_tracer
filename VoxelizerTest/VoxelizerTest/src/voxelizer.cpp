#include <algorithm>

#include <gl/glew.h>

#include <glm/gtx/transform.hpp>

#include "mesh.h"
#include "shader_helper.h"
#include "voxelizer.h"
#include "timer.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define THREAD_BUFFER_SIZE (1024 * 1024)
#define TEXTURE_SIZE_X (255) //resolutions may seem weird.
#define TEXTURE_SIZE_Y (255) //X and Y need to be divisible by 3
#define TEXTURE_SIZE_Z (129)
#define WORK_GROUP_SIZE 64
#define MAX_DEPTH 8

#define _BREAK_ON_GL_ERROR_                     \
{                                               \
  GLenum err = glGetError();                    \
  if(err != GL_NO_ERROR)                        \
  {                                             \
    printf("OpenGL error %X occurred!\n", err); \
    __debugbreak();                             \
  }                                             \
}

struct Node
{
  uint32_t child;
  uint32_t brick;
  uint neighbourXPos;
  uint neighbourXNeg;
  uint neighbourYPos;
  uint neighbourYNeg;
  uint neighbourZPos;
  uint neighbourZNeg;
};

struct StoredThread
{
  uint pos, diffuse, normal, emissive;
};

struct VoxelizerImpl
{
  GLuint m_albedo3DTexture = 0u;
  GLuint m_normal3DTexture = 0u;
  GLuint m_emission3DTexture = 0u;
  GLuint m_light3DTexture = 0u;

  GLuint m_shader;
  GLuint m_deferredShader;
  GLuint m_clearShader;
  GLuint m_setNeighbourPointersShader;
  GLuint m_mipmapShader;
  GLuint m_fillVBOsShader;
  GLuint m_insertLightShader;

  GLuint m_octreeSSBO;
  GLuint m_locksSSBO;
  GLuint m_storedThreadsSSBO[2];
  GLuint m_computeParamsSSBO[2];
  GLuint m_counters;
  GLuint m_computeParamsDIB;

  GLuint m_worldMatrixLocation;
  GLuint m_isTexturedLocation;
  GLuint m_textureLocation;
  GLuint m_colorLocation;
  GLuint m_imageLocation;
  GLuint m_resolutionLocation;

  int CreateSSBOs();
  int Create3DTexture(const glm::ivec3& voxelCount);
  int CreateShader();

  void DeleteSSBOs();
  void Delete3DTexture();
  void DeleteShader();
};

int VoxelizerImpl::CreateSSBOs()
{
  //create 2 new buffers
  glGenBuffers(1, &m_octreeSSBO);
  glGenBuffers(1, &m_locksSSBO);
  glGenBuffers(2, m_storedThreadsSSBO);
  glGenBuffers(2, m_computeParamsSSBO);
  glGenBuffers(1, &m_computeParamsDIB);

  //make these way to big for now
  const int octreeNodeCount = 1024 * 1024 * 6;
  const int storedThreadCount = THREAD_BUFFER_SIZE;

  //allocate data for the octree
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_octreeSSBO);
  glBufferData(
    GL_SHADER_STORAGE_BUFFER,
    octreeNodeCount * sizeof(Node),
    nullptr,
    GL_DYNAMIC_COPY);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_locksSSBO);
  glBufferData(
    GL_SHADER_STORAGE_BUFFER,
    sizeof(int32_t) * octreeNodeCount,
    nullptr,
    GL_DYNAMIC_COPY);

  for(int i = 0; i < 2; ++i)
  {
    //allocate data for stored threads
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_storedThreadsSSBO[i]);
    glBufferData(
      GL_SHADER_STORAGE_BUFFER,
      storedThreadCount * sizeof(StoredThread),
      nullptr,
      GL_DYNAMIC_COPY);
  }

  for(int i = 0; i < 2; ++i)
  {
    GLuint data[] = {0, 1, 1, 0, 0, 0};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_computeParamsSSBO[i]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(data), data, GL_DYNAMIC_COPY);
  }


  glGenBuffers(1, &m_counters);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_counters);
  glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint) * 2,
               nullptr, GL_DYNAMIC_DRAW);


  {
    GLuint data[] = {0, 1, 1};
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, m_computeParamsDIB);
    glBufferData(GL_DISPATCH_INDIRECT_BUFFER,
                 sizeof(data), data, GL_DYNAMIC_COPY);
  }


  //free the boobies, ehm buffers
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
  glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

  return 0;
}

int VoxelizerImpl::Create3DTexture(const glm::ivec3& voxelCount)
{
  
  uint32_t *asdf = (uint32_t*)malloc(
    TEXTURE_SIZE_X * TEXTURE_SIZE_Y *  TEXTURE_SIZE_Z * sizeof(uint32_t));
  for(int z = 0; z < TEXTURE_SIZE_Z; ++z)
  {
    for(int y = 0; y < TEXTURE_SIZE_Y; ++y)
    {
      for(int x = 0; x < TEXTURE_SIZE_X; ++x)
      {
        asdf[z * TEXTURE_SIZE_X * TEXTURE_SIZE_Y + y * TEXTURE_SIZE_X + x] = 0;
      }
    }
  }
  _BREAK_ON_GL_ERROR_;

  //generetate a new texture
  glGenTextures(1, &m_albedo3DTexture);
  _BREAK_ON_GL_ERROR_;
  glBindTexture(GL_TEXTURE_3D, m_albedo3DTexture);
  _BREAK_ON_GL_ERROR_;

  glTexImage3D(
    GL_TEXTURE_3D,    //target
    0,                //level
    GL_RGBA8,         //internalFormat
    TEXTURE_SIZE_X,   //width
    TEXTURE_SIZE_Y,   //height
    TEXTURE_SIZE_Z,   //depth
    0,                //border (must equal 0)
    GL_RGBA,          //format
    GL_UNSIGNED_BYTE, //type
    nullptr           //pixels
    );
  _BREAK_ON_GL_ERROR_;

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  //float color[] = {0.0f, 0.0f, 0.0f, 0.0f};
  //glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, color);
  
  _BREAK_ON_GL_ERROR_;

  free(asdf);

  glGenTextures(1, &m_normal3DTexture);
  glBindTexture(GL_TEXTURE_3D, m_normal3DTexture);

  glTexImage3D(
    GL_TEXTURE_3D,
    0,
    GL_RGBA8,
    TEXTURE_SIZE_X,
    TEXTURE_SIZE_Y,
    TEXTURE_SIZE_Z,
    0,
    GL_RGBA,
    GL_UNSIGNED_BYTE,
    nullptr);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);

  //generetate a new texture
  glGenTextures(1, &m_light3DTexture);
  glBindTexture(GL_TEXTURE_3D, m_light3DTexture);

  glTexImage3D(
    GL_TEXTURE_3D,    //target
    0,                //level
    GL_RGBA8,         //internalFormat   //this needs to become a different format later on
    TEXTURE_SIZE_X,     //width
    TEXTURE_SIZE_Y,     //height
    TEXTURE_SIZE_Z,     //depth
    0,                //border (must equal 0)
    GL_RGBA,          //format
    GL_UNSIGNED_BYTE, //type
    nullptr           //pixels
    );
  _BREAK_ON_GL_ERROR_;

  GLenum error;
  if((error = glGetError()) != GL_NO_ERROR)
  {
    printf("OpenGL error %X occured on glTexImage3D.\n", error);
    return 1;
  }


  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);

  glBindTexture(GL_TEXTURE_3D, 0);

  return 0;
}


int VoxelizerImpl::CreateShader()
{
  std::string fileData;
  const std::string sharedData =
    ReadFile("../assets/shaders/voxelization/shared.glsl");
  const char* data[] =
  {
    "#version 430 core\n",
    sharedData.c_str(),
    nullptr
  };
  

  //load an compile shaders
  GLuint voxelizationShaders[3];
  fileData = ReadFile("../assets/shaders/voxelization/voxelization.v.glsl");
  data[2] = fileData.c_str();
  voxelizationShaders[0] = CompileShader(data, 3, GL_VERTEX_SHADER);
  if(voxelizationShaders[0] == ~0u)
    return 1;
  fileData = ReadFile("../assets/shaders/voxelization/voxelization.f.glsl");
  data[2] = fileData.c_str();
  voxelizationShaders[1] = CompileShader(data, 3, GL_FRAGMENT_SHADER);
  if(voxelizationShaders[1] == ~0u)
    return 1;
  fileData = ReadFile("../assets/shaders/voxelization/voxelization.g.glsl");
  data[2] = fileData.c_str();
  voxelizationShaders[2] = CompileShader(data, 3, GL_GEOMETRY_SHADER);
  if(voxelizationShaders[2] == ~0u)
    return 1;

  //link them together
  m_shader = LinkShaders(voxelizationShaders, 3);

  //delete no longer needed shader files
  glDeleteShader(voxelizationShaders[2]);
  glDeleteShader(voxelizationShaders[1]);
  glDeleteShader(voxelizationShaders[0]);


  GLuint computeShader[1];
  fileData = ReadFile("../assets/shaders/voxelization/deferredVoxelization.c.glsl");
  data[2] = fileData.c_str();
  computeShader[0] = CompileShader(data, 3, GL_COMPUTE_SHADER);
  if(voxelizationShaders[0] == ~0u)
    return 1;
  m_deferredShader = LinkShaders(computeShader, 1);
  glDeleteShader(computeShader[0]);

  GLuint clearShaders[1];
  fileData = ReadFile("../assets/shaders/voxelization/clear_octree.c.glsl");
  data[2] = fileData.c_str();
  clearShaders[0] = CompileShader(data, 3, GL_COMPUTE_SHADER);
  if(clearShaders[0] == ~0u) return 1;
  m_clearShader = LinkShaders(clearShaders, 1);
  glDeleteShader(clearShaders[0]);

  GLuint setNeighbourPointers[1];
  fileData = ReadFile("../assets/shaders/voxelization/set_neighbour_pointers.c.glsl");
  data[2] = fileData.c_str();
  setNeighbourPointers[0] = CompileShader(data, 3, GL_COMPUTE_SHADER);
  if(setNeighbourPointers[0] == ~0u) return 1;
  m_setNeighbourPointersShader = LinkShaders(setNeighbourPointers, 1);
  glDeleteShader(setNeighbourPointers[0]);

  GLuint mipmapShaders[1];
  fileData = ReadFile("../assets/shaders/voxelization/generate_mipmap.c.glsl");
  data[2] = fileData.c_str();
  mipmapShaders[0] = CompileShader(data, 3, GL_COMPUTE_SHADER);
  if(mipmapShaders[0] == ~0u) return 1;
  m_mipmapShader = LinkShaders(mipmapShaders, 1);
  glDeleteShader(mipmapShaders[0]);
  _BREAK_ON_GL_ERROR_;

  GLuint fillVBOsShader[1];
  fileData = ReadFile("../assets/shaders/voxelization/generateVoxelVBO.c.glsl");
  data[2] = fileData.c_str();
  fillVBOsShader[0] = CompileShader(data, 3, GL_COMPUTE_SHADER);
  if(fillVBOsShader[0] == ~0u) return 1;
  m_fillVBOsShader = LinkShaders(fillVBOsShader, 1);
  glDeleteShader(fillVBOsShader[0]);
  _BREAK_ON_GL_ERROR_;

  GLuint insertLightShaders[1];
  fileData = ReadFile("../assets/shaders/voxelization/insert_light.c.glsl");
  data[2] = fileData.c_str();
  insertLightShaders[0] = CompileShader(data, 3, GL_COMPUTE_SHADER);
  if(insertLightShaders[0] == ~0u) return 1;
  m_insertLightShader = LinkShaders(insertLightShaders, 1);
  glDeleteShader(insertLightShaders[0]);
  _BREAK_ON_GL_ERROR_;

  m_worldMatrixLocation = glGetUniformLocation(m_shader, "u_worldMatrix");
  m_isTexturedLocation = glGetUniformLocation(m_shader, "u_isTextured");
  m_textureLocation = glGetUniformLocation(m_shader, "u_texture");
  m_colorLocation = glGetUniformLocation(m_shader, "u_color");
  m_imageLocation = glGetUniformLocation(m_shader, "u_image");
  m_resolutionLocation = glGetUniformLocation(m_shader, "u_resolution");
  return 0;
}


void VoxelizerImpl::DeleteSSBOs()
{
  //make sure they are not bound before deleting them
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


  if(m_counters)
  {
    glDeleteBuffers(1, &m_counters);
    m_counters = 0;
  }


  //delete the buffers
  if(m_storedThreadsSSBO)
  {
    glDeleteBuffers(2, m_storedThreadsSSBO);
    m_storedThreadsSSBO[0] = m_storedThreadsSSBO[1] = 0;
  }

  if(m_locksSSBO)
  {
    glDeleteBuffers(1, &m_locksSSBO);
    m_locksSSBO = 0;
  }

  if(m_octreeSSBO)
  {
    glDeleteBuffers(1, &m_octreeSSBO);
    m_octreeSSBO = 0;
  }
}
void VoxelizerImpl::Delete3DTexture()
{
  if(m_albedo3DTexture)
  {
    glDeleteTextures(1, &m_albedo3DTexture);
    m_albedo3DTexture = 0;
  }
}
void VoxelizerImpl::DeleteShader()
{
  if(m_deferredShader)
  {
    glDeleteProgram(m_deferredShader);
    m_deferredShader = 0;
  }
  if(m_shader)
  {
    glDeleteProgram(m_shader);
    m_shader = 0;
  }
}

Voxelizer::Voxelizer() : m_impl(new VoxelizerImpl) {}

Voxelizer::~Voxelizer() { Shutdown(); }

int Voxelizer::Init(const glm::ivec3& voxelCount, const glm::vec3& voxelSize)
{
  //make sure no OpenGL functions occured before calling this function
  GLenum error;
  if((error = glGetError()) != GL_NO_ERROR)
  {
    printf("Voxelizer::Init was called with a set OpenGL error.\n");
    return 1;
  }

  //set voxel count and voxel size
  m_voxelCount = voxelCount;
  m_voxelSize = voxelSize;

  int result = 0;

  if((result = m_impl->CreateSSBOs()) != 0)
    return result;

  if((result = m_impl->Create3DTexture(voxelCount)) != 0)
    return result;

  //const size_t size =
  //  voxelCount.x * voxelCount.y * voxelCount.z * sizeof(uint32_t);
  //if((result = m_impl->CreatePBO(size)) != 0)
  //  return result;

  if((result = m_impl->CreateShader()) != 0)
    return result;

  ClearOctree();

  return 0;
}

void Voxelizer::Shutdown()
{
  m_impl->DeleteSSBOs();
  m_impl->DeleteShader();
  m_impl->Delete3DTexture();
}

int Voxelizer::Voxelize(
  const std::vector<Mesh*> &m_meshes,
  const std::vector<glm::mat4>& worldTransforms,
  const glm::vec3& voxelSize,
  const glm::vec3& voxelOffset,
  uint idx)
{
  _BREAK_ON_GL_ERROR_;
  ClearOctree();

  Timer totalTimer;
  totalTimer.Start();

  _BREAK_ON_GL_ERROR_;


  //get current OpenGL state
  GLboolean oldDepthTest, oldCullFace, oldWriteMask[4], oldMultisample;
  int oldViewport[4];
  glGetBooleanv(GL_DEPTH_TEST, &oldDepthTest);
  glGetBooleanv(GL_CULL_FACE, &oldCullFace);
  glGetBooleanv(GL_MULTISAMPLE, &oldMultisample);
  glGetBooleanv(GL_COLOR_WRITEMASK, oldWriteMask);
  glGetIntegerv(GL_VIEWPORT, oldViewport);
  
  //set OpenGL state for voxelization
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDisable(GL_MULTISAMPLE);
  glViewport(0, 0, m_voxelCount.x, m_voxelCount.y);
  _BREAK_ON_GL_ERROR_;

  Timer timer;
  //voxelization
  timer.Start();
  const GLuint loc = glGetUniformLocation(m_impl->m_deferredShader, "u_limit");

  //bind shader program for voxelization
  glUseProgram(m_impl->m_shader);
  _BREAK_ON_GL_ERROR_;

  //calculate the scale that we need for the voxels
  glm::vec3 scaleVec = 4.0f / glm::vec3(
    (float)m_voxelCount.x * voxelSize.x,
    (float)m_voxelCount.y * voxelSize.y,
    (float)m_voxelCount.z * voxelSize.z);

  //set the resolution uniform
  glUniform1i(m_impl->m_resolutionLocation, m_voxelCount.x);
  _BREAK_ON_GL_ERROR_;


  //two pointers to two buffers, this makes swapping easier
  const GLuint* in = &m_impl->m_storedThreadsSSBO[0];
  const GLuint* out = &m_impl->m_storedThreadsSSBO[1];
  const GLuint* computeParamsIn = &m_impl->m_computeParamsSSBO[0];
  const GLuint* computeParamsOut = &m_impl->m_computeParamsSSBO[1];


  //bind buffers to the voxelization shader
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_impl->m_octreeSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_impl->m_locksSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, *in);
  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, m_impl->m_counters);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, *computeParamsIn);
  _BREAK_ON_GL_ERROR_;
  glBindImageTexture(0, m_impl->m_albedo3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  glBindImageTexture(1, m_impl->m_normal3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  _BREAK_ON_GL_ERROR_;

  //temporary static uint32_t to keep track of the maximum number of deferred
  // threads we have had so far
  static uint32_t maxThreads = 0;


  uint start = 0, end = (uint)m_meshes.size();
  if(idx != -1) start = idx, end = idx + 1;
  for(uint i = start; i < end; ++i)
  //for(uint i = 0; i < m_meshes.size(); ++i)
  {
    //calculate and send the transformation matrix
    const glm::mat4 mat = worldTransforms[i] * glm::scale(scaleVec);
    glUniformMatrix4fv(m_impl->m_worldMatrixLocation, 1, GL_FALSE, &mat[0][0]);

    //rasterize the mesh
    m_meshes[i]->Draw(
      m_impl->m_textureLocation,
      m_impl->m_isTexturedLocation,
      m_impl->m_colorLocation);
    //barrier up
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                    GL_ATOMIC_COUNTER_BARRIER_BIT |
                    GL_ALL_BARRIER_BITS);
    _BREAK_ON_GL_ERROR_;
  }


  glBindBuffer(GL_SHADER_STORAGE_BUFFER, *computeParamsIn);
  glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, m_impl->m_computeParamsDIB);
  glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER,
                      GL_DISPATCH_INDIRECT_BUFFER,
                      0, 0, 3 * sizeof(GLuint));
  _BREAK_ON_GL_ERROR_;
  
  /*
  const uint32_t* asdf = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER,
                                                GL_READ_ONLY);

  printf("%u %u\n", asdf[3], asdf[4]);
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  */
  //
  //Execute the remaining deferred threads
  //

  //bind the deferred shader to execute the remaining deferred threads
  glUseProgram(m_impl->m_deferredShader);

  //bind the necessary buffers to the shader
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_impl->m_octreeSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_impl->m_locksSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, *in);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, *out);
  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, m_impl->m_counters);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, *computeParamsIn);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, *computeParamsOut);

  _BREAK_ON_GL_ERROR_;
  glBindImageTexture(0, m_impl->m_albedo3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  glBindImageTexture(1, m_impl->m_normal3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  _BREAK_ON_GL_ERROR_;

#if 1
  //Run compute shaders for a certain amount of times.
  //This is an over estimation to prevent syncing the GPU and CPU.
  //Running compute shaders with a group size of 0 takes a
  // negligable amount time.
  //TODO create a safe implemntation that runs an x number of times, then checks
  // if there are any deferred threads to run, if so it runs x number of times
  // again. This introduces some syncronization, but it will be more safe
  for(int i = 0; i < 8; ++i)
  {
    _BREAK_ON_GL_ERROR_;

    //execute compute shader

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, m_impl->m_computeParamsDIB);
    glDispatchComputeIndirect(0);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                    GL_ATOMIC_COUNTER_BARRIER_BIT |
                    GL_ALL_BARRIER_BITS);
    _BREAK_ON_GL_ERROR_;


    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *computeParamsOut);
    glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        GL_DISPATCH_INDIRECT_BUFFER,
                        0, 0, 3 * sizeof(GLuint));
    _BREAK_ON_GL_ERROR_;

    //swap input and output buffer
    std::swap(in, out);
    std::swap(computeParamsIn, computeParamsOut);

    //rebind these, since they changed
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, *in);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, *out);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, *computeParamsIn);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, *computeParamsOut);
    _BREAK_ON_GL_ERROR_;
  }
#endif
  float voxelizationTime = timer.IntervalMS();
  //printf("voxelization took %.2f ms\n", voxelizationTime);


  timer.Start();
  {
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_impl->m_counters);
    const uint32_t* ptr = (uint32_t*)glMapBufferRange(
      GL_ATOMIC_COUNTER_BUFFER,
      0, sizeof(uint32_t) * 1,
      GL_MAP_READ_BIT);
    const uint32_t nodeCount = (ptr[0] - 1) * 8 + 1;
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    m_nodeCount = nodeCount;
    //printf("created %u nodes\n", m_nodeCount);
  }
  const float readingCounterTime = timer.IntervalMS();
  //printf("reading counter took %.2f ms\n", readingCounterTime);


  //reset the atomic counters
  timer.Start();
  {
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_impl->m_counters);
    uint32_t* ptr = (uint32_t*)glMapBufferRange(
      GL_ATOMIC_COUNTER_BUFFER,
      0, sizeof(uint32_t) * 1,
      GL_MAP_WRITE_BIT);
    ptr[0] = 0u;
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
  }
  const float resettingCounterTime = timer.IntervalMS();
  //printf("resetting counter took %.2f ms\n", resettingCounterTime);
  

  //const float totalTime = voxelizationTime + mipmappingTime + readingCounterTime + resettingCounterTime;
  //
  //printf("voxelizing took %.2f ms\n", totalTime);
  //printf("without reading %.2f ms\n", totalTime - readingCounterTime);
  //printf("reading counter %.2f ms\n", readingCounterTime);

  //reset OpenGL state
  if(oldDepthTest) glEnable(GL_DEPTH_TEST);
  if(oldCullFace) glEnable(GL_CULL_FACE);
  glColorMask(
    oldWriteMask[0], oldWriteMask[1], oldWriteMask[2], oldWriteMask[3]);
  if(oldMultisample) glEnable(GL_MULTISAMPLE);
  glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

  float time = totalTimer.IntervalMS();
  printf("voxelized scene in %.2f ms\n", time);
  return 0;
}

void Voxelizer::ClearOctree()
{
  if(m_nodeCount == 0)
    return;

  //bind the shader
  glUseProgram(m_impl->m_clearShader);

  //set the number of threads we want to execute, since exuction goes in groups
  glUniform1ui(
    glGetUniformLocation(m_impl->m_clearShader, "u_limit"),
    m_nodeCount);

  //bind the octree to the shader
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_impl->m_octreeSSBO);

  glBindImageTexture(0, m_impl->m_albedo3DTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glBindImageTexture(1, m_impl->m_normal3DTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glBindImageTexture(2, m_impl->m_light3DTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

  //execute the compute shader
  glDispatchCompute((m_nodeCount + 63 - 1) / 64, 1, 1);

  //barrier up just in case
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_impl->m_counters);
  uint32_t* ptr = (uint32_t*)glMapBufferRange(
    GL_ATOMIC_COUNTER_BUFFER,
    0, sizeof(uint32_t) * 2,
    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

  ptr[0] = 1u;
  ptr[1] = 0u;

  glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

}

uint Voxelizer::GetSSBO() { return m_impl->m_octreeSSBO; }
uint Voxelizer::GetDiffuse3DTex() { return m_impl->m_albedo3DTexture; }
uint Voxelizer::GetNormal3DTex() { return m_impl->m_normal3DTexture; }
uint Voxelizer::GetLight3DTex() { return m_impl->m_light3DTexture; }

uint Voxelizer::FillVoxelVBOs(GLuint posVBO, GLuint colorVBO, uint layer)
{
  //return 0;
  GLuint atomicCounter;
  glGenBuffers(1, &atomicCounter);
  _BREAK_ON_GL_ERROR_;

  //create a new atomic counter
  uint32_t zero = 0u;
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounter);
  glBufferData(GL_ATOMIC_COUNTER_BUFFER,
               sizeof(uint32_t), &zero, GL_DYNAMIC_DRAW);


  glBindBuffer(GL_ARRAY_BUFFER, posVBO);
  glBufferData(GL_ARRAY_BUFFER, m_nodeCount * 8 * sizeof(glm::vec3),
               nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
  glBufferData(GL_ARRAY_BUFFER, m_nodeCount * 8 * sizeof(GLuint),
               nullptr, GL_DYNAMIC_DRAW);


  //bind shader and bind resources
  glUseProgram(m_impl->m_fillVBOsShader);
  glUniform1ui(0, layer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_impl->m_octreeSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, posVBO);   //bind as SSBO
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, colorVBO); //bind as SSBO
  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, atomicCounter);
  _BREAK_ON_GL_ERROR_;
  glBindImageTexture(0, m_impl->m_light3DTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
  _BREAK_ON_GL_ERROR_;


  //run the shader
  const uint groupCount = ((512 >> (9 - layer)) + 3) / 4;
  glDispatchCompute(groupCount, groupCount, groupCount);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                  GL_ATOMIC_COUNTER_BARRIER_BIT |
                  GL_ALL_BARRIER_BITS);
  _BREAK_ON_GL_ERROR_;


  //get the counter
  uint32_t count;
  glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &count);

  glBindBuffer(GL_ARRAY_BUFFER, posVBO);
  glBindBuffer(GL_ARRAY_BUFFER, colorVBO);

  //unbind the buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
  _BREAK_ON_GL_ERROR_;


  //delete these one time use buffers
  glDeleteBuffers(1, &atomicCounter);
  _BREAK_ON_GL_ERROR_;

  return count;
}

void Voxelizer::InsertLighting(GLuint shadowMap, glm::ivec2 resolution,
                               const glm::mat4& invLightMatrix,
                               const glm::vec3& lightForward)
{
  GLuint computeShader = m_impl->m_insertLightShader;
  GLuint loc;

  glUseProgram(computeShader);


  loc = glGetUniformLocation(computeShader, "u_invMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &invLightMatrix[0][0]);

  loc = glGetUniformLocation(computeShader, "u_lightVector");
  glUniform3fv(loc, 1, &lightForward[0]);

  glm::vec3 scaleVec = 4.0f / glm::vec3(
    (float)m_voxelCount.x * m_voxelSize.x,
    (float)m_voxelCount.y * m_voxelSize.y,
    (float)m_voxelCount.z * m_voxelSize.z);
  glm::mat4 worldSpaceToVoxelSpaceMatrix = glm::scale(scaleVec);
  loc = glGetUniformLocation(computeShader, "u_worldSpaceToVoxelSpaceMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &worldSpaceToVoxelSpaceMatrix[0][0]);


  //bind the shadow/depth map
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, shadowMap);
  glUniform1i(glGetUniformLocation(computeShader, "u_depthmap"), 5);
  

  //GLuint loc = glGetUniformLocation(computeShader, "u_image");
  //glUniform1i(loc, 0);
  //_BREAK_ON_GL_ERROR_;

  //bind the texture bricks
  glBindImageTexture(0, m_impl->m_albedo3DTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
  glBindImageTexture(1, m_impl->m_normal3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  glBindImageTexture(2, m_impl->m_light3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

  //bind the octree SSBO
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_impl->m_octreeSSBO);
  _BREAK_ON_GL_ERROR_;

  //run the compute shader
  glDispatchCompute(resolution.x / 32, resolution.y / 32, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS); //barrier up!
}

void Voxelizer::GenMipmaps()
{
  static float t = 0;
  static int c = -20;
  
  Timer timer;
  timer.Start();
  
  //MIP map the SVO
  glUseProgram(m_impl->m_mipmapShader);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_impl->m_octreeSSBO);
  glBindImageTexture(0, m_impl->m_albedo3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  glBindImageTexture(1, m_impl->m_normal3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  glBindImageTexture(2, m_impl->m_light3DTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
  for(int i = 1; i < MAX_DEPTH; ++i)
  {
    glUniform1ui(glGetUniformLocation(m_impl->m_mipmapShader, "u_layer"), MAX_DEPTH - i - 1);
    const uint size = glm::max(m_voxelCount.x >> (i + 1), 1);
    uint groupCount = (size + 3) / 4;
    glDispatchCompute(groupCount, groupCount, groupCount);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
  }

  //sync with the CPU for timing purposes
  //TODO remove this!
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_impl->m_counters);
  glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_READ_WRITE);
  glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

  if(++c > 0)
  {
    t += timer.IntervalMS();
    //printf("mipmapping time: %.4f ms\n", t / (float)c);
  }
}
