#include "app.h"
#include "input.h"
#include "timer.h"
#include "morton.h"
#include "shader_helper.h"
#include "voxelizer.h"

#include <vld.h>

App::App() {}
App::~App() {}


#define _BREAK_ON_GL_ERROR_                     \
{                                               \
  GLenum err = glGetError();                    \
  if(err != GL_NO_ERROR)                        \
  {                                             \
    printf("OpenGL error 0x%x occurred!\n", err);\
    __debugbreak();                             \
  }                                             \
}

template<typename T> T Min(const T& a, const T& b) { return a < b ? a : b; }
template<typename T> T Max(const T& a, const T& b) { return a > b ? a : b; }
template<typename T> T Clamp(const T& v, const T& min, const T& max)
{
  return Max(min, Min(max, v));
}

template<typename T>
T Min(const T& a, const T& b, const T& c) { return Min(a, Min(b, c)); }
template<typename T>
T Min(const T& a, const T& b, const T& c, const T& d)
{ return Min(Min(a, b), Min(c, d)); }

template<typename T>
T Max(const T& a, const T& b, const T& c) { return Max(a, Max(b, c)); }
template<typename T>
T Max(const T& a, const T& b, const T& c, const T& d)
{ return Max(Max(a, b), Max(c, d)); }

struct CubeVert { glm::vec3 pos; glm::vec3 norm; };

static CubeVert cubeVertices[] =
{
  {glm::vec3(-.5f, -.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},  // 0
  {glm::vec3(0.5f, -.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},  // 1
  {glm::vec3(-.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},  // 2
  {glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},  // 3
  {glm::vec3(-.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f)},  // 4
  {glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f)},  // 5
  {glm::vec3(-.5f, 0.5f, -.5f), glm::vec3(0.0f, 1.0f, 0.0f)},  // 6
  {glm::vec3(0.5f, 0.5f, -.5f), glm::vec3(0.0f, 1.0f, 0.0f)},  // 7

  {glm::vec3(-.5f, 0.5f, -.5f), glm::vec3(0.0f, 0.0f, -1.0f)}, // 8
  {glm::vec3(0.5f, 0.5f, -.5f), glm::vec3(0.0f, 0.0f, -1.0f)}, // 9
  {glm::vec3(-.5f, -.5f, -.5f), glm::vec3(0.0f, 0.0f, -1.0f)}, //10
  {glm::vec3(0.5f, -.5f, -.5f), glm::vec3(0.0f, 0.0f, -1.0f)}, //11
  {glm::vec3(-.5f, -.5f, -.5f), glm::vec3(0.0f, -1.0f, 0.0f)}, //12
  {glm::vec3(0.5f, -.5f, -.5f), glm::vec3(0.0f, -1.0f, 0.0f)}, //13
  {glm::vec3(-.5f, -.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f)}, //14
  {glm::vec3(0.5f, -.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f)}, //15

  {glm::vec3(0.5f, -.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f)},  //16
  {glm::vec3(0.5f, -.5f, -.5f), glm::vec3(1.0f, 0.0f, 0.0f)},  //17
  {glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f)},  //18
  {glm::vec3(0.5f, 0.5f, -.5f), glm::vec3(1.0f, 0.0f, 0.0f)},  //19
  {glm::vec3(-.5f, -.5f, -.5f), glm::vec3(-1.0f, 0.0f, 0.0f)}, //20
  {glm::vec3(-.5f, -.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f)}, //21
  {glm::vec3(-.5f, 0.5f, -.5f), glm::vec3(-1.0f, 0.0f, 0.0f)}, //22
  {glm::vec3(-.5f, 0.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f)}  //23
};

static uint32_t cubeIndices[] =
{
  0, 1, 2,    // 0
  2, 1, 3,    // 1
  4, 5, 6,    // 2
  6, 5, 7,    // 3
  8, 9, 10,   // 4
  10, 9, 11,  // 5
  12, 13, 14, // 6
  14, 13, 15, // 7
  16, 17, 18, // 8
  18, 17, 19, // 9
  20, 21, 22, //10
  22, 21, 23  //11
};


int App::FillVoxelGrid()
{
  std::vector<glm::mat4> worldTransforms;
  worldTransforms.reserve(m_meshes.size());
  for(size_t i = 0; i < m_meshes.size(); ++i)
    worldTransforms.push_back(glm::mat4(1.0f));

  //only time the actual voxelization
  Timer timer;
  timer.Start();
  m_voxelizer.Voxelize(
    m_meshes,
    worldTransforms,
    glm::vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z),
    glm::vec3(0, 0, 0));
  float voxelizationTime = timer.IntervalMS();
  printf("voxelized in %.2f ms\n", voxelizationTime);
  glEnable(GL_MULTISAMPLE);

  //copy all voxels over in a format for rendering
  SetOctreeDisplayLevel(_tzcnt_u32(VOXEL_COUNT_X));

  return 0;
}

void App::SetOctreeDisplayLevel(int level)
{

  m_voxels.clear();
  m_voxelColors.clear();

  m_currentOctreeDisplayLevel = level;

  if(level < 0)
    return;

  m_voxelCount = m_voxelizer.FillVoxelVBOs(m_voxelPosBuffer, m_voxelColorBuffer, level);

  //glBegin(GL_TRIANGLES); glEnd();

  /*
  m_tree.FillPosAndColorBuffer(level, m_voxels, m_voxelColors);

  //transform the voxels
  for(auto &v : m_voxels)
  {
    v += -glm::vec3(VOXEL_COUNT_X / 2, VOXEL_COUNT_Y / 2, VOXEL_COUNT_Z / 2)
      + glm::vec3(.5f, .5f, .5f);
    v *= glm::vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z);
  }

  UploadVoxels();
  */
}

int App::LoadSponza()
{
  std::vector<std::vector<Vertex>> vertexArrays;
  std::vector<std::vector<uint32_t>> indexArrays;
  std::vector<Texture*> textures;
  std::vector<glm::vec3> colors;

  int result;

#if 1
  result = LoadOBJ(
    "../assets/dragon.obj",
    vertexArrays,
    indexArrays,
    textures,
    colors);

  if(result != 0)
  {
    printf("Failed to load OBJ!\n");
    return 1;
  }

  m_meshes.reserve(m_meshes.size() + vertexArrays.size());

  for(size_t i = 0; i < vertexArrays.size(); ++i)
  {
    //for(auto &v : vertexArrays[i])
    //  v.position += glm::vec3(-0.1f, 0.1f, -0.1f);
    
    Mesh* mesh = new Mesh;
    mesh->Init(std::move(vertexArrays[i]), std::move(indexArrays[i]));
    mesh->SetTexture(textures[i]);
    mesh->SetColor(glm::vec4(colors[i], 1));
    //mesh->SetColor(glm::vec4(1, .5f, 1, 1));


    char buffer[64];
    _itoa_s((int32_t)i, buffer, 10);
    mesh->SetName(
      std::string("../assets/dragon.obj") +
      std::string("#") + std::string(buffer));

    m_meshes.push_back(mesh);
  }
  vertexArrays.clear();
  indexArrays.clear();
  textures.clear();
  colors.clear();
#endif

#if 0
  result = LoadOBJ(
    "../assets/triangle.obj",
    vertexArrays,
    indexArrays,
    textures,
    colors);

  if(result != 0)
  {
    printf("Failed to load OBJ!\n");
    return 1;
  }
  //vertexArrays[0][0].position = glm::vec3(0, 10.0f, 0);
  //vertexArrays[0][3].position = glm::vec3(0, 10.0f, 0);
  //for(auto& v : vertexArrays[0])
  //  std::swap(v.position.z, v.position.x);

  //indexArrays[0][0] = indexArrays[0][3];
  //indexArrays[0][1] = indexArrays[0][4];
  //indexArrays[0][2] = indexArrays[0][5];
  //indexArrays[0].resize(3);

  m_meshes.reserve(m_meshes.size() + vertexArrays.size());

  for(size_t i = 0; i < vertexArrays.size(); ++i)
  {
    Mesh* mesh = new Mesh;
    mesh->Init(std::move(vertexArrays[i]), std::move(indexArrays[i]));
    mesh->SetTexture(textures[i]);
    mesh->SetColor(glm::vec4(colors[i], 1));

    char buffer[64];
    _itoa_s((int32_t)i, buffer, 10);
    mesh->SetName(
      std::string("../assets/triangle.obj") +
      std::string("#") + std::string(buffer));

    m_meshes.push_back(mesh);
  }
  vertexArrays.clear();
  indexArrays.clear();
  textures.clear();
  colors.clear();
#endif
#if 1
  //load the OBJ file
  result = LoadOBJ(
    "../assets/sponza/sponza.obj",
    vertexArrays,
    indexArrays,
    textures,
    colors);

  //check if loading was successful
  if(result != 0)
  {
    printf("Failed to load OBJ!\n");
    return 1;
  }

  m_meshes.reserve(m_meshes.size() + vertexArrays.size());

  for(size_t i = 0; i < vertexArrays.size(); ++i)
  {
    Mesh* mesh = new Mesh;
    mesh->Init(std::move(vertexArrays[i]), std::move(indexArrays[i]));
    mesh->SetTexture(textures[i]);

    char buffer[64];
    _itoa_s((int32_t)i, buffer, 10);
    mesh->SetName(
      std::string("../assets/sponza/sponza.obj") +
      std::string("?") + std::string(buffer));
    //mesh->SetColor(glm::vec4(colors[i], 1));
    m_meshes.push_back(mesh);
  }

  vertexArrays.clear();
  indexArrays.clear();
  textures.clear();
  colors.clear();
#endif

  return 0;
}

int App::SetupVoxelRendering()
{

  _BREAK_ON_GL_ERROR_;
  glGenVertexArrays(1, &m_voxelVAO);
  glBindVertexArray(m_voxelVAO);

  //upload positions and normals
  glGenBuffers(1, &m_cubeVBO);
  glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVert), (void*)offsetof(CubeVert, pos));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVert), (void*)offsetof(CubeVert, norm));

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  _BREAK_ON_GL_ERROR_;

  //setup voxel positions
  glGenBuffers(1, &m_voxelPosBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_voxelPosBuffer);
  glBufferData(
    GL_ARRAY_BUFFER,
    1 * sizeof(glm::vec3),
    nullptr,
    GL_STATIC_DRAW);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  //setup voxel colors
  glGenBuffers(1, &m_voxelColorBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_voxelColorBuffer);
  glBufferData(
    GL_ARRAY_BUFFER,
    1 * sizeof(uint32_t),
    nullptr,
    GL_STATIC_DRAW);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);

  _BREAK_ON_GL_ERROR_;
  //unbind the last bound array buffer
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenBuffers(1, &m_cubeEBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  glVertexAttribDivisor(0, 0); // voxel positions
  glVertexAttribDivisor(1, 0); // voxel normals
  glVertexAttribDivisor(2, 1); // positions
  glVertexAttribDivisor(3, 1); // color

  glBindVertexArray(0);
  return 0;
}

int App::Init()
{
  _BREAK_ON_GL_ERROR_

    Light light;
  light.color = glm::vec3(1, 1, 1);
  light.direction = glm::normalize(glm::vec3(0.3, 1, 0.1));// glm::normalize(glm::vec3(-0.3f, -0.8f, 0.6f));
  light.enabled = true;
  light.intensity = 1.0f;
  light.lightType = 0;
  light.position = glm::vec3(0, 0, 0);
  m_lights.push_back(light);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  _BREAK_ON_GL_ERROR_

  SetupVoxelRendering();
  LoadSponza();
  int result = InitShadowmaps();
  if(result != 0)
  {
    printf("error %d occured initing shadowmaps.\n", result);
    return result;
  }
  std::string fileData;
  GLuint voxelShaders[2];
  fileData = ReadFile("../assets/shaders/voxel_shader.v.glsl");
  voxelShaders[0] = CompileShader(fileData.c_str(), GL_VERTEX_SHADER);
  fileData = ReadFile("../assets/shaders/voxel_shader.f.glsl");
  voxelShaders[1] = CompileShader(fileData.c_str(), GL_FRAGMENT_SHADER);
  m_voxelShader = LinkShaders(voxelShaders, 2);
  glDeleteShader(voxelShaders[1]);
  glDeleteShader(voxelShaders[0]);
  _BREAK_ON_GL_ERROR_;

  const std::string sharedData =
    ReadFile("../assets/shaders/voxelization/shared.glsl");
  const char* data[3] =
  {
    "#version 430 core\n",
    sharedData.c_str(),
    nullptr
  };
  GLuint meshShaders[2];
  fileData = ReadFile("../assets/shaders/mesh_shader.v.glsl");
  data[2] = fileData.c_str();
  meshShaders[0] = CompileShader(data, 3, GL_VERTEX_SHADER);
  fileData = ReadFile("../assets/shaders/mesh_shader.f.glsl");
  data[2] = fileData.c_str();
  meshShaders[1] = CompileShader(data, 3, GL_FRAGMENT_SHADER);
  m_meshShader = LinkShaders(meshShaders, 2);
  glDeleteShader(meshShaders[1]);
  glDeleteShader(meshShaders[0]);
  _BREAK_ON_GL_ERROR_;

  GLuint shadowShaders[2];
  fileData = ReadFile("../assets/shaders/shadow_mapping.v.glsl");
  shadowShaders[0] = CompileShader(fileData.c_str(), GL_VERTEX_SHADER);
  if(shadowShaders[0] == ~0u)
    return 1;
  _BREAK_ON_GL_ERROR_;

  fileData = ReadFile("../assets/shaders/shadow_mapping.f.glsl");
  shadowShaders[1] = CompileShader(fileData.c_str(), GL_FRAGMENT_SHADER);
  if(shadowShaders[0] == ~0u)
    return 1;
  _BREAK_ON_GL_ERROR_;

  m_shadowShader = LinkShaders(shadowShaders, 2);
  for(int i = 1; i >= 0; --i)
    glDeleteShader(shadowShaders[i]);


  //GLuint computeShaders[1];
  //fileData = ReadFile("../assets/shaders/test.c.glsl");
  //computeShaders[0] = CompileShader(fileData.c_str(), GL_COMPUTE_SHADER);
  //if(computeShaders[0] == ~0u)
  //  return 1;

  //m_computeShader = LinkShaders(computeShaders, 1);
  //for(int i = 0; i >= 0; --i)
  //  glDeleteShader(computeShaders[i]);

  result = m_voxelizer.Init(
    glm::ivec3(VOXEL_COUNT_X, VOXEL_COUNT_Y, VOXEL_COUNT_Z),
    glm::vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z));
  if(result != 0)
  {
    printf("error %d occrued initalizing the voxelizer!\n", result);
    return result;
  }

  result = FillVoxelGrid();
  if(result != 0)
  {
    printf("error %d occured filling the voxel grid!\n", result);
    return result;
  }


  //m_camera.position = glm::vec3(0, 0, 2);
  //m_camera.position = glm::vec3(0, 2, 0);
  m_camera.position = glm::vec3(-8.607288f, 8.566921f, -1.409398f);

  glm::vec3 minV = glm::vec3(1e34f, 1e34f, 1e34f);
  glm::vec3 maxV = glm::vec3(-1e34f, -1e34f, -1e34f);

  for(auto& m : m_meshes)
  {
    glm::vec3 extends[2];
    m->GetExtends(extends);

    if(extends[0].x < minV.x) minV.x = extends[0].x;
    if(extends[0].y < minV.y) minV.y = extends[0].y;
    if(extends[0].z < minV.z) minV.z = extends[0].z;

    if(extends[1].x > maxV.x) maxV.x = extends[1].x;
    if(extends[1].y > maxV.y) maxV.y = extends[1].y;
    if(extends[1].z > maxV.z) maxV.z = extends[1].z;
  }
    
  return 0;
}
int App::Shutdown()
{
  glUseProgram(0);
  glDeleteProgram(m_shadowShader);
  glDeleteProgram(m_voxelShader);
  glDeleteProgram(m_meshShader);
  //glDeleteProgram(m_voxelizationShader);

  m_voxelizer.Shutdown();
  //m_tree.Shutdown();
  return 0;
}

int App::Update(float dt)
{
  printf("frame time: %.2f ms\n", dt * 1000.0f);

#if 0
  //static uint idx = 0;
  //if(input::GetKeyDown(SDL_SCANCODE_COMMA))
  //{
  //  if(idx == 0)
  //    idx = (uint)m_meshes.size() - 1;
  //  else
  //    --idx;
  //}
  //if(input::GetKeyDown(SDL_SCANCODE_PERIOD))
  //  idx = (idx + 1) % m_meshes.size();

  //if(input::GetKeyDown(SDL_SCANCODE_U))
  //{
  //  std::vector<glm::mat4> worldTransforms;
  //  worldTransforms.reserve(m_meshes.size());
  //  for(size_t i = 0; i < m_meshes.size(); ++i)
  //    worldTransforms.push_back(glm::mat4(1.0f));
  //
  //  int result = m_voxelizer.Voxelize(
  //    m_meshes,
  //    worldTransforms,
  //    glm::vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z),
  //    glm::vec3(0, 0, 0)
  //    );
  //
  //  if(result != 0)
  //    return result;
  //  //SetOctreeDisplayLevel(m_currentOctreeDisplayLevel);
  //}
#endif
  //static float rx = 0, ry =  .5f * (float)M_PI;
  static float rx = -0.670626f, ry =  4.552595f;

  //static float rx = -.5f * M_PI, ry = M_PI;
  float speed = 10.0f;
  if(input::GetKey(SDL_SCANCODE_LSHIFT)) speed = 1.0f;

  printf("%f %f\n", m_lx, m_lz);
  
  glm::vec3 movement = glm::vec3(0, 0, 0);
  if(input::GetKey(SDL_SCANCODE_W)/*w*/) movement += glm::vec3(0, 0, -1) * dt;
  if(input::GetKey(SDL_SCANCODE_S)/*s*/) movement += glm::vec3(0, 0, 1) * dt;
  if(input::GetKey(SDL_SCANCODE_A)/*a*/) movement += glm::vec3(-1, 0, 0) * dt;
  if(input::GetKey(SDL_SCANCODE_D)/*d*/) movement += glm::vec3(1, 0, 0) * dt;
  if(input::GetKey(SDL_SCANCODE_Q)/*q*/) movement += glm::vec3(0, -1, 0) * dt;
  if(input::GetKey(SDL_SCANCODE_E)/*e*/) movement += glm::vec3(0, 1, 0) * dt;
  movement *= speed;

  if(input::GetKey(SDL_SCANCODE_UP)/*up*/) rx -= dt;
  if(input::GetKey(SDL_SCANCODE_DOWN)/*down*/) rx += dt;
  if(input::GetKey(SDL_SCANCODE_LEFT)/*left*/) ry += dt;
  if(input::GetKey(SDL_SCANCODE_RIGHT)/*right*/) ry -= dt;

  if(input::GetKeyDown(SDL_SCANCODE_MINUS))
  {
    SetOctreeDisplayLevel(Max(m_currentOctreeDisplayLevel - 1, 0));
    printf("level: %d\n", m_currentOctreeDisplayLevel);
  }
  if(input::GetKeyDown(SDL_SCANCODE_EQUALS))
  {
    const int newLevel = Min(
      m_currentOctreeDisplayLevel + 1,
      (int)_tzcnt_u32(VOXEL_COUNT_X));

    SetOctreeDisplayLevel(newLevel);
    printf("level: %d\n", m_currentOctreeDisplayLevel);
  }

  if(input::GetKeyDown(SDL_SCANCODE_F))
    m_wireframe = !m_wireframe;

  glm::quat qx(cosf(rx / 2), sinf(rx / 2), 0, 0);
  glm::quat qy(cosf(ry / 2), 0, sinf(ry / 2), 0);

  glm::quat rotation = qy * qx;
  movement = rotation * movement;

  m_camera.rotation = rotation;
  m_camera.position += movement;

  if(input::GetKeyDown(SDL_SCANCODE_V)) m_renderVoxels = !m_renderVoxels;
  if(input::GetKeyDown(SDL_SCANCODE_M)) m_renderMesh = !m_renderMesh;
  if(input::GetKeyDown(SDL_SCANCODE_G)) m_enableGI = !m_enableGI;

  if(input::GetKey(SDL_SCANCODE_COMMA)) m_lambda -= 0.1f * dt;
  if(input::GetKey(SDL_SCANCODE_PERIOD)) m_lambda += 0.1f * dt;
  printf("lambda: %f\n", m_lambda);

  if(input::GetKey(SDL_SCANCODE_J)) m_lx -= dt * speed * .1f;
  if(input::GetKey(SDL_SCANCODE_L)) m_lx += dt * speed * .1f;
  if(input::GetKey(SDL_SCANCODE_I)) m_lz += dt * speed * .1f;
  if(input::GetKey(SDL_SCANCODE_K)) m_lz -= dt * speed * .1f;

  glm::quat qlx(cosf(m_lx / 2), sin(m_lx / 2), 0, 0);
  glm::quat qly(cosf(m_ly / 2), 0, sin(m_ly / 2), 0);
  glm::quat qlz(cosf(m_lz / 2), 0, 0, sin(m_lz / 2));
  glm::quat ql = qlz * qly * qlx;

  m_lights[0].direction = ql * glm::vec3(0, 0, -1);


  return 0;
}

void App::UploadVoxels()
{
  //upload voxel positions to GPU
  glBindBuffer(GL_ARRAY_BUFFER, m_voxelPosBuffer);
  glBufferData(
    GL_ARRAY_BUFFER, m_voxels.size() * sizeof(glm::vec3), m_voxels.data(), GL_STATIC_DRAW);
  _BREAK_ON_GL_ERROR_;

  glBindBuffer(GL_ARRAY_BUFFER, m_voxelColorBuffer);
  glBufferData(
    GL_ARRAY_BUFFER, m_voxelColors.size() * sizeof(uint32_t), m_voxelColors.data(), GL_STATIC_DRAW);
  _BREAK_ON_GL_ERROR_;


  glBindBuffer(GL_ARRAY_BUFFER, 0);
  _BREAK_ON_GL_ERROR_;
}

void App::RenderVoxels()
{
  if(m_wireframe)
  {
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  //bind VAO and shader
  glBindVertexArray(m_voxelVAO);
  glUseProgram(m_voxelShader);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const uint32_t maxLevel = _tzcnt_u32(VOXEL_COUNT_X);

  float voxelLevelScale = (float)pow(2, maxLevel - m_currentOctreeDisplayLevel);

  //calculate matrices
  glm::mat4 modelMatrix = glm::scale(
    glm::vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z) * voxelLevelScale);
  glm::mat4 worldMatrix(1.0f);
  glm::mat4 viewMatrix = m_camera.ToViewMatrix();
  glm::mat4 projectionMatrix = glm::perspectiveFov<float>(60.0f, SCRWIDTH, SCRHEIGHT, 0.1f, 1000000.0f);
  glm::mat4 wvpMatrix = projectionMatrix * viewMatrix * worldMatrix;


  _BREAK_ON_GL_ERROR_;


  //update uniforms
  GLuint loc;
  loc = glGetUniformLocation(m_voxelShader, "u_modelMatrix");
  if(loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &modelMatrix[0][0]);
  loc = glGetUniformLocation(m_voxelShader, "u_worldMatrix");
  if(loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &worldMatrix[0][0]);
  loc = glGetUniformLocation(m_voxelShader, "u_viewMatrix");
  if(loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &viewMatrix[0][0]);
  loc = glGetUniformLocation(m_voxelShader, "u_projectionMatrix");
  if(loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &projectionMatrix[0][0]);
  loc = glGetUniformLocation(m_voxelShader, "u_wvpMatrix");
  if(loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &wvpMatrix[0][0]);

  _BREAK_ON_GL_ERROR_;


  //bind index buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeEBO);

  //draw
  glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT,
                          nullptr, (GLsizei)m_voxelCount);
  //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

  //unbind index buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  //unbind program and VAO
  glUseProgram(0);
  glBindVertexArray(0);

  glEnable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  glDisable(GL_BLEND);
}

void App::RenderShadowmap()
{
  _BREAK_ON_GL_ERROR_;
  const Shadowmap& shadowmap = m_shadowmap;
  const GLuint shader = m_shadowShader;
  for(auto &light : m_lights)
  {
    glUseProgram(shader);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowmap.framebuffer);
    //glClearDepth(0.0f);
    glClear(GL_DEPTH_BUFFER_BIT); //clear the shadow map from the last frame
    //glClearDepth(1.0f);
    glDrawBuffer(GL_NONE);
    glDepthRange(0.0f, 1.0f);
    glViewport(0, 0, shadowmap.width, shadowmap.height);
    _BREAK_ON_GL_ERROR_;

    glm::mat4 worldMatrix = glm::scale(glm::vec3(1.0f));
    glm::mat4 viewProjMatrix = light.GenerateLightMatrix();

    _BREAK_ON_GL_ERROR_;

    GLuint loc;
    loc = glGetUniformLocation(shader, "u_worldMatrix");
    if(loc != ~0u) glUniformMatrix4fv(loc, 1, GL_FALSE, &worldMatrix[0][0]);
    _BREAK_ON_GL_ERROR_;

    loc = glGetUniformLocation(shader, "u_viewProjMatrix");
    if(loc != ~0u) glUniformMatrix4fv(loc, 1, GL_FALSE, &viewProjMatrix[0][0]);
    _BREAK_ON_GL_ERROR_;


    for(auto &mesh : m_meshes)
      mesh->Draw(~0u, ~0u, ~0u);
    _BREAK_ON_GL_ERROR_;

    glm::mat4 invViewProjMat = glm::inverse(viewProjMatrix);

  #if 0
    //insert lighting into octree
    glUseProgram(m_computeShader);

    _BREAK_ON_GL_ERROR_;
    loc = glGetUniformLocation(m_computeShader, "u_invMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &invViewProjMat[0][0]);

    glm::vec3 scaleVec = 4.0f / glm::vec3(
      (float)VOXEL_COUNT_X * VOXEL_SIZE_X,
      (float)VOXEL_COUNT_Y * VOXEL_SIZE_Y,
      (float)VOXEL_COUNT_Z * VOXEL_SIZE_Z);
    glm::mat4 worldSpaceToVoxelSpaceMatrix = glm::scale(scaleVec);
    loc = glGetUniformLocation(m_computeShader, "u_worldSpaceToVoxelSpaceMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &worldSpaceToVoxelSpaceMatrix[0][0]);

    
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, shadowmap.depthmap);
    glUniform1i(glGetUniformLocation(m_computeShader, "u_depthmap"), 5);
    glUniform1f(glGetUniformLocation(m_computeShader, "u_depth"), .775f);
    loc = glGetUniformLocation(m_computeShader, "u_image");
    glUniform1i(loc, 0);

    _BREAK_ON_GL_ERROR_;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_voxelizer.GetSSBO());
    _BREAK_ON_GL_ERROR_;

    glDispatchCompute(shadowmap.width / 32, shadowmap.height / 32, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    

    glBindTexture(GL_TEXTURE_2D, 0);
  #endif
    m_voxelizer.InsertLighting(
      shadowmap.depthmap,
      glm::ivec2(shadowmap.width, shadowmap.height),
      invViewProjMat, light.direction);

    SetOctreeDisplayLevel(m_currentOctreeDisplayLevel);


    _BREAK_ON_GL_ERROR_;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, SCRWIDTH, SCRHEIGHT);
  _BREAK_ON_GL_ERROR_;
}
void App::RenderMesh()
{
  if(m_wireframe)
  {
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  glUseProgram(m_meshShader);

  glm::mat4 worldMatrix = glm::scale(glm::vec3(1.0f));
  glm::mat4 viewMatrix = m_camera.ToViewMatrix();
  glm::mat4 projectionMatrix = glm::perspectiveFov<float>(60.0f, SCRWIDTH, SCRHEIGHT, 0.1f, 1000000.0f);
  glm::mat4 wvpMatrix = projectionMatrix * viewMatrix * worldMatrix;
  glm::mat4 lightMatrix = m_lights[0].GenerateLightMatrix();

  GLuint loc;
  loc = glGetUniformLocation(m_meshShader, "u_worldMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &worldMatrix[0][0]);

  loc = glGetUniformLocation(m_meshShader, "u_viewMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &viewMatrix[0][0]);

  loc = glGetUniformLocation(m_meshShader, "u_projectionMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &projectionMatrix[0][0]);

  loc = glGetUniformLocation(m_meshShader, "u_wvpMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &wvpMatrix[0][0]);

  loc = glGetUniformLocation(m_meshShader, "u_lightMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &lightMatrix[0][0]);

  loc = glGetUniformLocation(m_meshShader, "u_lightDir");
  glUniform3fv(loc, 1, &m_lights[0].direction[0]);

  loc = glGetUniformLocation(m_meshShader, "u_layer");
  glUniform1ui(loc, m_currentOctreeDisplayLevel);

  loc = glGetUniformLocation(m_meshShader, "u_lambda");
  glUniform1f(loc, m_lambda);

  _BREAK_ON_GL_ERROR_;
  loc = glGetUniformLocation(m_meshShader, "u_enableGI");
  glUniform1ui(loc, m_enableGI);
  _BREAK_ON_GL_ERROR_;

  GLuint isTexturedLoc = glGetUniformLocation(m_meshShader, "u_isTextured");
  glUniform1i(isTexturedLoc, 0);

  GLuint textureLoc = glGetUniformLocation(m_meshShader, "u_texture");

  GLuint colorLoc = glGetUniformLocation(m_meshShader, "u_color");
  glUniform4f(colorLoc, .5f, 0, 1, 1);

  //calculate the scale that we need for the voxels
  glm::vec3 scaleVec = 4.0f / glm::vec3(
    (float)VOXEL_COUNT_X * VOXEL_SIZE_X,
    (float)VOXEL_COUNT_Y * VOXEL_SIZE_Y,
    (float)VOXEL_COUNT_Z * VOXEL_SIZE_Z);
  const glm::mat4 voxelMatrix = glm::scale(scaleVec);

  loc = glGetUniformLocation(m_meshShader, "u_voxelMatrix");
  glUniformMatrix4fv(loc, 1, GL_FALSE, &voxelMatrix[0][0]);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_shadowmap.depthmap);
  //glUniform1i(glGetUniformLocation(m_meshShader, "u_depthmap"), 1);

  glBindImageTexture(0, m_voxelizer.GetDiffuse3DTex(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_3D, m_voxelizer.GetDiffuse3DTex());
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_3D, m_voxelizer.GetNormal3DTex());
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_3D, m_voxelizer.GetLight3DTex());

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_voxelizer.GetSSBO());

  for(auto &mesh : m_meshes)
  {
    mesh->Draw(textureLoc, isTexturedLoc, colorLoc);
  }

  _BREAK_ON_GL_ERROR_;

  glUseProgram(0);
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

int App::Render()
{
  
  _BREAK_ON_GL_ERROR_;

  //clear screen buffer
  glClearColor(0.4f, 0.52f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  _BREAK_ON_GL_ERROR_;

  //if(input::GetKeyDown(SDL_SCANCODE_U))
  {
    std::vector<glm::mat4> worldTransforms;
    worldTransforms.reserve(m_meshes.size());
    for(size_t i = 0; i < m_meshes.size(); ++i)
      worldTransforms.push_back(glm::mat4(1.0f));

    int result = m_voxelizer.Voxelize(
      m_meshes,
      worldTransforms,
      glm::vec3(VOXEL_SIZE_X, VOXEL_SIZE_Y, VOXEL_SIZE_Z),
      glm::vec3(0, 0, 0)
      );

    if(result != 0)
      return result;
  }

  RenderShadowmap();
  _BREAK_ON_GL_ERROR_;
  
  m_voxelizer.GenMipmaps();
  
   

  SetOctreeDisplayLevel(m_currentOctreeDisplayLevel);

  if(m_renderVoxels)
    RenderVoxels();

  if(m_renderMesh)
    RenderMesh();
  _BREAK_ON_GL_ERROR_;
  
  return 0;
}

int App::InitShadowmaps()
{
  m_shadowmap.width = 2084;
  m_shadowmap.height = 2048;
  glGenFramebuffers(1, &m_shadowmap.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, m_shadowmap.framebuffer);

  glGenTextures(1, &m_shadowmap.depthmap);
  glBindTexture(GL_TEXTURE_2D, m_shadowmap.depthmap);
  glTexImage2D(GL_TEXTURE_2D,        //target
               0,                    //level
               GL_DEPTH_COMPONENT32, //internal format
               m_shadowmap.width,    //width
               m_shadowmap.height,   //height
               0,                    //border (must equal 0)
               GL_DEPTH_COMPONENT,   //format
               GL_FLOAT,             //type
               nullptr);             //pixels

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //TODO set to border color
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_shadowmap.depthmap, 0);
  glDrawBuffer(GL_NONE);

  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return 1;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}
