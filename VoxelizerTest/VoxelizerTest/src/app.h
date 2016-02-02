#pragma once

#define SCRWIDTH  1280
#define SCRHEIGHT 720

#define VOXEL_COUNT_X 256
#define VOXEL_COUNT_Y 256
#define VOXEL_COUNT_Z 256

#define VOXEL_SIZE_X (0.33333333f) //(0.33333333f * 4.0f)
#define VOXEL_SIZE_Y (0.33333333f) //(0.33333333f * 4.0f)
#define VOXEL_SIZE_Z (0.33333333f) //(0.33333333f * 4.0f)
                     
#include <gl\glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <vector>

#include "transform.h"
#include "types.h"
#include "obj_loader.h"
#include "mesh.h"
#include "voxelizer.h"
#include "light.h"



struct Shadowmap
{
  int width, height;
  GLuint framebuffer;
  GLuint depthmap;
};

class App
{
public:
  App();
  ~App();

  int Init();
  int Shutdown();

  int Update(float deltaTime);
  int Render();
private:

  int LoadSponza();
  int SetupVoxelRendering();

  void RenderVoxels();
  void RenderMesh();

  int InitShadowmaps();
  void RenderShadowmap();

  int FillVoxelGrid();
  void UploadVoxels();
  void SetOctreeDisplayLevel(int level);

  GLuint m_voxelShader;
  GLuint m_meshShader;
  //GLuint m_voxelizationShader;


  std::vector<glm::vec3> m_voxels;
  std::vector<uint> m_voxelColors;

  std::vector<Light> m_lights;

  GLuint m_voxelVAO;
  GLuint m_voxelPosBuffer;
  GLuint m_voxelColorBuffer;

  GLuint m_cubeVBO;
  GLuint m_cubeEBO;

  Transform m_camera;

  bool m_renderMesh = true;
  bool m_renderVoxels = false;
  bool m_wireframe = false;
  bool m_enableGI = true;
  float m_lambda = 1.0f;
  //GLuint m_3DTexture;

  int m_currentOctreeDisplayLevel = -1;

  std::vector<Mesh*> m_meshes;

  //Octree m_tree;

  GLuint m_shadowShader;
  Shadowmap m_shadowmap;

  //GLuint m_computeShader;
  
  Voxelizer m_voxelizer;

  uint32_t m_voxelCount = 0;

  //float m_lx = -1.5f, m_ly = 0.0f, m_lz = -.5f;
  float m_lx = -1.846146f, m_ly = 0.0f, m_lz = 0.402122f;
};
