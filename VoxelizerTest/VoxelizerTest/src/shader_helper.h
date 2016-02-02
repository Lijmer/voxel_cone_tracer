#pragma once
#include <string>
#include <fstream>
#include <vector>

#include <gl/glew.h>

#include "types.h"

//shady shizzle

inline std::string ReadFile(const char* filename)
{
  std::string result;

  //open file
  std::ifstream file(filename);

  //read file if it exists
  if(file.is_open())
  {
    result = std::string(
      (std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());
  }
  else
    printf("Failed to read file: [%s]\n", filename);

  return result;
}

inline uint CompileShader(const char** data, int fileCount, GLuint shaderType)
{
  //compile the shader
  GLuint shader = glCreateShader(shaderType);
  glShaderSource(shader, fileCount, data, nullptr);
  glCompileShader(shader);

  //check the shader
  GLint result = GL_FALSE;
  GLint infoLogLen = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if(result != GL_TRUE)
  {
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
    std::vector<char> errorMessage(infoLogLen + 1);
    glGetShaderInfoLog(shader, infoLogLen, nullptr, &errorMessage[0]);
    printf("%s\n", errorMessage.data());
    return ~0u;
  }
  return shader;
}

inline uint CompileShader(const char* data, GLuint shaderType)
{
  //compile the shader
  GLuint shader = glCreateShader(shaderType);
  glShaderSource(shader, 1, &data, nullptr);
  glCompileShader(shader);

  //check the shader
  GLint result = GL_FALSE;
  GLint infoLogLen = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if(result != GL_TRUE)
  {
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
    std::vector<char> errorMessage(infoLogLen + 1);
    glGetShaderInfoLog(shader, infoLogLen, nullptr, &errorMessage[0]);
    printf("%s\n", errorMessage.data());
    return ~0u;
  }
  return shader;
}

inline uint LinkShaders(GLuint* shaders, uint shaderCount)
{
  uint program = glCreateProgram();
  for(uint i = 0; i < shaderCount; ++i)
    glAttachShader(program, shaders[i]);
  glLinkProgram(program);

  // Check the program
  GLint result = GL_FALSE;
  GLint infoLogLen = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if(result != GL_TRUE)
  {
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
    std::vector<char> errorMessage(infoLogLen + 1);
    glGetProgramInfoLog(program, infoLogLen, NULL, &errorMessage[0]);
    printf("%s\n", &errorMessage[0]);
    return ~0u;
  }
  return program;
}
