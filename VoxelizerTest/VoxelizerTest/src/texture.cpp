#include "texture.h"
#include <cstdlib> //_aligned_malloc
#include <cstring> //memcpy
#include <fstream> //std::ifstream

#include <FreeImage.h>

int Texture::Init(const char* filepath)
{
  //todo load texture from file
  //try to open and close file, to check if it exists
  std::ifstream testFile(filepath);
  if(!testFile.is_open())
    return 1;
  testFile.close();

  //get file type
  FREE_IMAGE_FORMAT fileType = FreeImage_GetFIFFromFilename(filepath);
  if(fileType == FIF_UNKNOWN) fileType = FreeImage_GetFileType(filepath);
  if(fileType == FIF_UNKNOWN)
    return 1;

  //load image
  FIBITMAP* tmp = FreeImage_Load(fileType, filepath);
  if(tmp == nullptr)
    return 1;

  //convert image to a 32 bit image
  FIBITMAP* bmp = FreeImage_ConvertTo32Bits(tmp);
  FreeImage_Unload(tmp);
  if(bmp == nullptr)
    return 1;

  //set width, height and name
  m_width = FreeImage_GetWidth(bmp);
  m_height = FreeImage_GetHeight(bmp);

  //allocate pixel buffer
  const size_t size = m_width * m_height * sizeof(uint32_t);
  m_pixels = (uint32_t*)_aligned_malloc(size, 64);
  if(m_pixels == nullptr)
    return 1;

  //copy pixels over
  //NOTE loaded bitmap is upside down
  //uint32_t* a = m_pixels;
  //for(int y = m_height - 1; y >= 0; --y)
  //{
  //  BYTE* scanline = FreeImage_GetScanLine(bmp, y);
  //  memcpy(a, scanline, m_width * sizeof(uint32_t));
  //  a += m_width;
  //}

  memcpy(m_pixels, FreeImage_GetBits(bmp), size);

  //clean
  FreeImage_Unload(bmp);

  //upload to GPU
  int result = UploadToGL();
  if(result != 0)
    return result;

  return 0;
}

int Texture::Init(const uint32_t* pixels, uint32_t width, uint32_t height)
{
  //helper values
  const uint32_t pixelCount = width * height;
  const uint32_t size = pixelCount * sizeof(uint32_t);

  //allocate pixels
  m_pixels = (uint32_t*)_aligned_malloc(size, 64);
  if(m_pixels == nullptr)
    return 1;

  //copy pixels over
  memcpy(m_pixels, pixels, size);

  //set width and height member variables
  m_width = width;
  m_height = height;


  //upload texture to GPU
  int result = UploadToGL();
  if(result != 0)
    return result;

  //success
  return 0;
}

void Texture::Shutdown()
{
  _aligned_free(m_pixels);
  m_pixels = nullptr;

  if(m_glTexture)
  {
    glDeleteTextures(1, &m_glTexture);
    m_glTexture = 0;
  }
}

int Texture::UploadToGL()
{
  glGenTextures(1, &m_glTexture);
  glBindTexture(GL_TEXTURE_2D, m_glTexture);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0,
    GL_BGRA, GL_UNSIGNED_BYTE, m_pixels);

  glGenerateMipmap(GL_TEXTURE_2D);

  //set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  //glTexParameteri(GL_TEXTURE_2D, )

  glBindTexture(GL_TEXTURE_2D, 0);
  return 0;
}

