#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "non_copyable.h"

class Mesh;

struct VoxelizerImpl;
class Voxelizer
{
public:
  Voxelizer();
  ~Voxelizer();
  //allocates memory required for voxelizing
  int Init(const glm::ivec3& voxelCount, const glm::vec3& voxelSize);
  void Shutdown();
  
  int Voxelize(
    const std::vector<Mesh*> &m_meshes,
    const std::vector<glm::mat4>& worldTransforms,
    const glm::vec3& voxelSize,
    const glm::vec3& voxelOffset,
    uint idx = -1);

  void ClearOctree();

  uint GetSSBO();
  uint GetDiffuse3DTex();
  uint GetNormal3DTex();
  uint GetLight3DTex();
  uint GetNodeCount() const { return m_nodeCount; }

  uint FillVoxelVBOs(GLuint posVBO, GLuint colorVBO, uint layer);

  void InsertLighting(GLuint shadowMap, glm::ivec2 resolution,
                      const glm::mat4& invLightMatrix,
                      const glm::vec3& lightForward);

  void GenMipmaps();

private:
  glm::ivec3 m_voxelCount;
  glm::vec3 m_voxelSize;
  std::unique_ptr<VoxelizerImpl> m_impl;
  uint32_t m_nodeCount = 1;
 
};
