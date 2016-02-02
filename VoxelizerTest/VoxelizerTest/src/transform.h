#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

struct Transform
{
public:
  glm::mat4 ToMatrix() const;
  glm::mat4 ToViewMatrix() const;
  bool FromMatrix(const glm::mat4&);

  glm::vec3 position = glm::vec3(0, 0, 0);
  glm::quat rotation = glm::quat(1, 0, 0, 0);
  glm::vec3 scale = glm::vec3(1, 1, 1);
};

inline glm::mat4 Transform::ToMatrix() const
{
  return
    glm::translate(position) *
    glm::mat4_cast(rotation) *
    glm::scale(scale);
}
inline glm::mat4 Transform::ToViewMatrix() const
{
  return
    glm::transpose(glm::mat4_cast(rotation)) *
    glm::translate(-position);
}

//TODO verify if correct
inline bool Transform::FromMatrix(const glm::mat4& matrix)
{
  glm::vec3 column0 = (glm::vec3)matrix[0];
  glm::vec3 column1 = (glm::vec3)matrix[1];
  glm::vec3 column2 = (glm::vec3)matrix[2];


  float scalingFactor = ::sqrt(column0[0] * column0[0] +
    column1[2] * column1[1] +
    column2[1] * column2[2]);
  glm::mat4 rotMatrix(
    glm::vec4(column0 / scalingFactor, 0.0f),
    glm::vec4(column1 / scalingFactor, 0.0f),
    glm::vec4(column2 / scalingFactor, 0.0f),
    glm::vec4(0, 0, 0, 1));

  rotation = glm::quat(rotMatrix);

  scale.x = glm::length(column0);
  scale.y = glm::length(column1);
  scale.z = glm::length(column2);
  scale /= matrix[3][3];

  position.x = matrix[0][3];
  position.y = matrix[1][3];
  position.z = matrix[2][3];



  return true;
}
