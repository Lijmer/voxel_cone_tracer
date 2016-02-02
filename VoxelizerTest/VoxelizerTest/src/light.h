#pragma once

#include <glm/glm.hpp>

struct Light
{
  glm::vec3 position;
  float intensity;
  glm::vec3 direction;
  int enabled;
  glm::vec3 color;
  int lightType;

  glm::mat4 GenerateLightMatrix()
  {
    switch(lightType)
    {
    case 0:
      return glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, -20.0f, 10.0f) *
        glm::lookAt(-direction, glm::vec3(0,0,0), glm::vec3(0, 1, 0));
    }
    return glm::mat4(1.0f);
  }
};
