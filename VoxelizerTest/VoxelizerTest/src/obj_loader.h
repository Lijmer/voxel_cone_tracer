#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "types.h"
#include "vertex.h"
#include "texture.h"

void LoadOBJ(const std::string &filename,
  std::vector<glm::vec3> &verts,
  std::vector<glm::vec3> &normals,
  std::vector<glm::vec2> &texcoords,
  std::vector<int> &vertIndices,
  std::vector<int> &normalIndices,
  std::vector<int> &texcoordIndices);

int LoadOBJ(const std::string &filename,
  std::vector<std::vector<Vertex>> &vertexArrays,
  std::vector<std::vector<uint32_t>> &indexArrays,
  std::vector<Texture*> &textures,
  std::vector<glm::vec3> &colors);

void LoadOBJ(const std::string &filename,
  std::vector<Vertex> &vertices,
  std::vector<uint32_t> &indices);
