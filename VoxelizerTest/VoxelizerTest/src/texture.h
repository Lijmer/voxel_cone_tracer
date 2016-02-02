#pragma once

#include <cstdint>
#include <gl\glew.h>

class Texture
{
public:
  Texture() {}
  ~Texture() { Shutdown(); }

#pragma region(copy and move stuff)
  Texture(const Texture&) = delete;
  Texture(Texture&& t) :
    m_pixels(t.m_pixels),
    m_width(t.m_width),
    m_height(t.m_height),
    m_glTexture(t.m_glTexture) 
  {
    t.m_pixels = nullptr;
    t.m_width = t.m_height = t.m_glTexture = 0;
  }
  const Texture& operator=(const Texture&) = delete;
  const Texture& operator=(Texture&& t)
  {
    m_pixels = t.m_pixels;
    m_width = t.m_width;
    m_height = t.m_height;
    m_glTexture = t.m_glTexture;

    t.m_pixels = nullptr;
    t.m_width = t.m_height = t.m_glTexture = 0;

    return *this;
  }
#pragma endregion


  int Init(const char* filepath);
  int Init(const uint32_t* pixels, uint32_t width, uint32_t height);

  void Shutdown();

  void Bind(int i) const
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, m_glTexture);
  }
  static void Unbind(int i)
  {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

private:

  int UploadToGL();

  uint32_t* m_pixels = nullptr;
  uint32_t m_width = 0;
  uint32_t m_height = 0;
  GLuint m_glTexture = 0;
};
