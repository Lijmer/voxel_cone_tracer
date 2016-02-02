#pragma once

#include <vector>

#include <gl/glew.h>

#include "vertex.h"
#include "types.h"

class Texture;

struct Material
{
  glm::vec4 color = glm::vec4(1, 1, 1, 1);
  Texture* texture = nullptr;
};


class Mesh
{
public:
  Mesh() {}
  ~Mesh() { Shutdown(); }

#pragma region(copy and move stuff)
  Mesh(const Mesh&) = delete;
  Mesh(Mesh&& m) :
    m_vertices(std::move(m.m_vertices)),
    m_indices(std::move(m.m_indices)),
    m_vao(m.m_vao),
    m_vbo(m.m_vbo),
    m_ibo(m.m_ibo),
    m_material(m.m_material)
  {
    m.m_vao = m.m_vbo = m.m_ibo = 0;
  }

  const Mesh& operator=(const Mesh&) = delete;
  const Mesh& operator=(Mesh&& m)
  {
    m_vertices = std::move(m.m_vertices);
    m_indices = std::move(m.m_indices);
    m_vao = m.m_vao;
    m_vbo = m.m_vbo;
    m_ibo = m.m_ibo;
    m_material = m.m_material;

    m.m_vao = m.m_vbo = m.m_ibo = 0;

    return *this;
  }
#pragma endregion

  int Init(std::vector<Vertex> &&vertices,
    std::vector<uint32_t> &&indices);

  void Shutdown();

  void Draw(
    GLuint textureLoc,
    GLuint isTexturedLoc,
    GLuint colorLoc) const;

  void Voxelize() const;

  void SetTexture(Texture* tex) { m_material.texture = tex; }
  void SetColor(const glm::vec4 &col) { m_material.color = col; }

  void GetExtends(glm::vec3 out[2]);

  const std::string& GetName() const { return m_name; }
  void SetName(const std::string& str) { m_name = str; }

  uint GetIndexCount() const { return (uint)m_indices.size(); }

private:
  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;

  GLuint m_vao = 0;
  GLuint m_vbo = 0;
  GLuint m_ibo = 0;

  Material m_material;

  std::string m_name;
};
