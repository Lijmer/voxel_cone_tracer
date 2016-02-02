#pragma once

#include "types.h"
#include <glm/glm.hpp>

#include <vector>

class Octree
{
public:

  struct Node
  {
    uint32_t child;
    uint32_t color;
  };

  void Init(const uint32_t* denseVoxels, int sizeX, int sizeY, int sizeZ);

  void Shutdown() { m_nodes.clear(); }

  const Node* GetRoot() const { return &m_nodes[0]; }
  const std::vector<Node>& GetNodess() const { return m_nodes; }
  std::vector<Node>& GetNodes() { return m_nodes; }


  void FillPosAndColorBuffer(
    int targetLevel,
    std::vector<glm::vec3>& posBuffer, std::vector<uint32_t> &colorBuffer) const;

  void SetSize(int x)
  {
    m_sizeX = m_sizeY = m_sizeZ = x;
    m_pitchY = m_sizeX;
    m_pitchZ = m_pitchY * m_sizeY;
  }

  void CheckTree();

private:

  void FillPosAndColorBuffer_R(
    const Node* node, int x, int y, int z, int level, int targetLevel,
    std::vector<glm::vec3>& posBuffer, std::vector<uint32_t> &colorBuffer) const;
  
  std::vector<Node> m_nodes;
  int m_pitchZ, m_pitchY;
  int m_sizeX, m_sizeY, m_sizeZ;
  int m_childCount = 0;
};
