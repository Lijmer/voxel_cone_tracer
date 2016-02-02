#include "mesh.h"
#include "types.h"
#include "texture.h"

int Mesh::Init(
  std::vector<Vertex> &&vertices,
  std::vector<uint32_t> &&indices)
{
  //move vertices and indices
  m_vertices = std::move(vertices);
  m_indices = std::move(indices);

  //create and bind vao
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);

  //create vbo
  glGenBuffers(1, &m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex),
    m_vertices.data(), GL_STATIC_DRAW);

  //enable vertex attributes
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  //set vertex attribute pointers
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
    sizeof(Vertex), (void*)offsetof(Vertex, position));

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
    sizeof(Vertex), (void*)offsetof(Vertex, normal));

  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
    sizeof(Vertex), (void*)offsetof(Vertex, texcoord));

  //unbind buffer
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  //create ibo
  glGenBuffers(1, &m_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t),
    m_indices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  //unbind vao
  glBindVertexArray(0);

  return 0;
}

void Mesh::Shutdown()
{
  m_vertices.clear();
  m_vertices.shrink_to_fit();

  m_indices.clear();
  m_indices.shrink_to_fit();

  if(m_vao)
  {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }

  if(m_vbo)
  {
    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;
  }

  if(m_ibo)
  {
    glDeleteBuffers(1, &m_ibo);
    m_ibo = 0;
  }
}

void Mesh::Draw(GLuint textureLoc, GLuint isTexturedLoc, GLuint colorLoc) const
{
  //bind vao for vertex attribute bindings
  glBindVertexArray(m_vao);

  //set isTexture boolean
  int isTextured = m_material.texture != nullptr;
  glUniform1i(isTexturedLoc, isTextured);
  
  //set texture slot and bind texture, if there is one
  if(isTextured)
  {
    m_material.texture->Bind(0);
    glUniform1i(textureLoc, 0);
  }

  //set material color
  glUniform4fv(colorLoc, 1, &m_material.color[0]);

  //draw the mesh
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, nullptr);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  //unbind the texture
  Texture::Unbind(0);

  //unbind vao
  glBindVertexArray(0);
}

void Mesh::Voxelize() const
{
  //bind vao for vertex attribute bindings
  glBindVertexArray(m_vao);

  //draw the mesh
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, nullptr);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  //unbind the texture
  Texture::Unbind(0);

  //unbind vao
  glBindVertexArray(0);
}

void Mesh::GetExtends(glm::vec3 out[2])
{
  glm::vec3 minV = m_vertices[0].position;
  glm::vec3 maxV = m_vertices[0].position;

  for(int i = 1; i < m_vertices.size(); ++i)
  {
    const glm::vec3& v = m_vertices[i].position;

    if(v.x < minV.x) minV.x = v.x;
    if(v.y < minV.y) minV.y = v.y;
    if(v.z < minV.z) minV.z = v.z;

    if(v.x > maxV.x) maxV.x = v.x;
    if(v.y > maxV.y) maxV.y = v.y;
    if(v.z > maxV.z) maxV.z = v.z;
  }

  out[0] = minV;
  out[1] = maxV;
}
