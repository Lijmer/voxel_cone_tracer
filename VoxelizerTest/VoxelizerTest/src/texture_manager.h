#pragma once

#include <map>

#include "singleton.h"
#include "texture.h"

class TextureManager : public Singleton<TextureManager>
{
public:
  
  Texture* GetTexture(const std::string& value)
  {
    //look for the texture
    auto it = m_textures.find(value);
    
    //if we found it, return a pointer
    if(it != m_textures.end())
      return it->second;

    //otherwise we have to load it
    Texture *tex = new Texture;
    if(tex == nullptr)
      return nullptr;

    int result = tex->Init(value.c_str());
    if(result != 0)
    {
      delete tex;
      tex = nullptr;
    }

    m_textures[value] = tex;

    return tex;
  }

  ~TextureManager()
  {
    for(auto &i : m_textures)
    {
      delete i.second;
    }
  }
  

private:
  std::map<std::string, Texture*> m_textures;
};
