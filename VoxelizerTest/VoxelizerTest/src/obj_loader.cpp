#define _CRT_SECURE_NO_WARNINGS //shut up MSVC


#include <string>

#include "obj_loader.h"

#include <algorithm>
#include <map>
#include <memory>

#include "texture_manager.h"

bool operator<(const Vertex& l, const Vertex &r)
{
  return memcmp((void*)&l, (void*)&r, sizeof(Vertex)) > 0;
};

//void GenerateTangentsAndBitangents(std::vector<Vertex> vertices,
//                                   std::vector<uint>)
//{
//
//}

//based n: http://www.opengl-tutorial.org/beginners-tutorials/tutorial-7-model-loading/
void LoadOBJ(const std::string &filename,
             std::vector<glm::vec3> &verts,
             std::vector<glm::vec3> &normals,
             std::vector<glm::vec2> &texcoords,
             std::vector<int> &vertIndices,
             std::vector<int> &normalIndices,
             std::vector<int> &texcoordIndices)
{
  FILE * file = nullptr;
  errno_t ret = fopen_s(&file, filename.c_str(), "rb");
  if(ret != 0)
  {
    printf("error opening file: %d\n", ret);
    return;
  }

  for(;;)
  {
    char lineHeader[128];
    int res = fscanf(file, "%s", lineHeader);
    if(res == EOF)
      break;

    if(strcmp(lineHeader, "v") == 0) //vertices
    {
      glm::vec3 vertex;
      fscanf_s(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
      vertex.x = -vertex.x;
      verts.push_back(vertex);
    }
    else if(strcmp(lineHeader, "vt") == 0)
    {
      glm::vec2 texcoord;
      fscanf_s(file, "%f %f\n", &texcoord.x, &texcoord.y);
      texcoords.push_back(texcoord);
    }
    else if(strcmp(lineHeader, "vn") == 0) //normals
    {
      glm::vec3 normal;
      fscanf_s(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
      normals.push_back(normal);
    }
    else if(strcmp(lineHeader, "f") == 0) //faces
    {
      std::string v1, v2, v3;
      unsigned int vertexIndex[3], texcoordIndex[3], normalIndex[3];
      int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n",
                           &vertexIndex[0], &texcoordIndex[0], &normalIndex[0],
                           &vertexIndex[1], &texcoordIndex[1], &normalIndex[1],
                           &vertexIndex[2], &texcoordIndex[2], &normalIndex[2]);
      if(matches != 9)
      {
        printf("File can't be read with this simple parser\n");
        return;
      }
      vertIndices.push_back(vertexIndex[0] - 1);
      vertIndices.push_back(vertexIndex[2] - 1);
      vertIndices.push_back(vertexIndex[1] - 1);
      normalIndices.push_back(normalIndex[0] - 1);
      normalIndices.push_back(normalIndex[2] - 1);
      normalIndices.push_back(normalIndex[1] - 1);
      texcoordIndices.push_back(texcoordIndex[0] - 1);
      texcoordIndices.push_back(texcoordIndex[2] - 1);
      texcoordIndices.push_back(texcoordIndex[1] - 1);
    }
  }

}

static void IndexVertices(std::vector<Vertex>& verts,
                          std::vector<uint32_t>& indices)
{

  std::vector<Vertex> input = verts;
  verts.clear();
  std::map<Vertex, uint32_t> vertexToOutIndex;
  for(uint i = 0; i < input.size(); ++i)
  {
    const Vertex& vertex = input[i];
    std::map<Vertex, uint32_t>::iterator it = vertexToOutIndex.find(vertex);
    if(it != vertexToOutIndex.end())
      indices.push_back(it->second);
    else
    {
      verts.push_back(vertex);
      uint32_t newIndex = (uint32_t)verts.size() - 1;
      indices.push_back(newIndex);
      vertexToOutIndex[vertex] = newIndex;
    }
  }
}

std::string ResolvePath(const std::string& filename)
{
  size_t posForward = filename.find_last_of('/');
  size_t posBackward = filename.find_last_of('\\');
  size_t pos = std::string::npos;
  if(posForward != std::string::npos)
    pos = posForward;
  if(posBackward != std::string::npos)
    pos = std::max(posForward, posBackward);
  if(pos == std::string::npos)
    return std::string("");

  return filename.substr(0, pos + 1);
}


struct OBJMaterial
{
  std::string name;
  glm::vec3 ambientColor = glm::vec3(1, 1, 1);
  glm::vec3 diffuseColor = glm::vec3(1, 1, 1);
  glm::vec3 specularColor = glm::vec3(1, 1, 1);

  glm::vec3 transmissionFilter = glm::vec3(1, 1, 1);

  std::string ambientTex;
  std::string diffuseTex;
  std::string specularTex;
  std::string bumpMap;

  int illum;
  float specularExponent;
  float refractionIndex;
  float transparency;
  float sharpness;

};

struct CachedMaterial
{
  CachedMaterial() {}
  CachedMaterial(OBJMaterial& m)
  {
    strcpy(diffuseTex, m.diffuseTex.c_str());
    diffuseColor = m.diffuseColor;
  }
  char diffuseTex[_MAX_PATH];
  glm::vec3 diffuseColor;
};

static uint ParseFace(const char* line, uint* v, uint* t, uint* n)
{
  const char* delim = " \t\n\r";
  char tokens[0x100];
  strcpy(tokens, line);
  const char* str = strtok(tokens, delim);

  int format = 0;
  for(size_t i = 0, len = strlen(tokens); i < len - 1; ++i)
  {
    if(format == 0 && tokens[i] == '/') format = 1;
    if(tokens[i] == '/' && tokens[i + 1] == '/') format = 2;
  }


  uint verts = 0;
  uint vars;

  switch(format)
  {
  case 0:
    for(int i = 0; i < 16; ++i)
    {
      v[verts] = atoi(str);
      ++verts;
      if((str = strtok(nullptr, delim)) == nullptr)
        break;
    }
    break;

  case 1:
    vars = sscanf(str, "%d/%d/%d", &v[verts], &t[verts], &n[verts]);
    if(vars == 3)
    {
      ++verts;
      if((str = strtok(nullptr, delim)) == nullptr)
        break;

      for(int i = 1; i < 16; ++i)
      {
        vars = sscanf(str, "%d/%d/%d", &v[verts], &t[verts], &n[verts]);
        ++verts;
        if(vars != 3 || (str = strtok(nullptr, delim)) == nullptr)
          break;
      }
    }
    else
    {
      vars = sscanf(str, "%d/%d", &v[verts], &t[verts]);
      if(vars == 2)
      {
        ++verts;
        if((str = strtok(nullptr, delim)) == nullptr)
          break;

        for(int i = 1; i < 16; ++i)
        {
          vars = sscanf(str, "%d/%d", &v[verts], &t[verts]);
          ++verts;
          if(vars != 2 || (str = strtok(nullptr, delim)) == nullptr)
            break;
        }
      }
    }
    break;
  case 2:
    for(int i = 0; i < 16; ++i)
    {
      vars = sscanf(str, "%d//%d", &v[verts], &n[verts]);
      ++verts;
      if(vars != 2 || (str = strtok(nullptr, delim)) == nullptr)
        break;
    }
    break;

  default:
    return 0xffffffff;
  }
  return verts;
}

static int LoadMaterials(
  const std::string& basePath,
  const std::string& filename,
  std::vector<OBJMaterial>& out_materials)
{
  FILE* file = fopen(filename.c_str(), "rb");
  if(file == nullptr)
    return 1;

  std::shared_ptr<FILE> safe_file(file, &fclose);
  char line[0x100];

  OBJMaterial tmpMaterial;
  bool inMaterial = false;
  for(; fgets(line, sizeof(line), file) != nullptr;)
  {
    char* token = strtok(line, " \t\n\r");
    if(token == nullptr || token[0] == '#')
      continue;
    else if(strcmp(token, "newmtl") == 0)
    {
      if(inMaterial)
      {
        out_materials.push_back(tmpMaterial);
        tmpMaterial = OBJMaterial();
      }
      inMaterial = true;
      tmpMaterial.name = strtok(nullptr, " \t\n\r");
    }
    else if(strcmp(token, "Ka") == 0)
    {
      glm::vec3 c(glm::vec3::_null);
      //NOTE order needs to be forced
      c.x = (float)atof(strtok(nullptr, " \t\n\r"));
      c.y = (float)atof(strtok(nullptr, " \t\n\r"));
      c.z = (float)atof(strtok(nullptr, " \t\n\r"));
      tmpMaterial.ambientColor = c;
    }
    else if(strcmp(token, "Kd") == 0)
    {
      glm::vec3 c(glm::vec3::_null);
      c.x = (float)atof(strtok(nullptr, " \t\n\r"));
      c.y = (float)atof(strtok(nullptr, " \t\n\r"));
      c.z = (float)atof(strtok(nullptr, " \t\n\r"));
      tmpMaterial.diffuseColor = c;
    }
    else if(strcmp(token, "Ks") == 0)
    {
      glm::vec3 c(glm::vec3::_null);
      c.x = (float)atof(strtok(nullptr, " \t\n\r"));
      c.y = (float)atof(strtok(nullptr, " \t\n\r"));
      c.z = (float)atof(strtok(nullptr, " \t\n\r"));
      tmpMaterial.specularColor = c;
    }
    else if(strcmp(token, "map_Ka") == 0)
      tmpMaterial.ambientTex = basePath + strtok(nullptr, "\t\n\r");
    else if(strcmp(token, "map_Kd") == 0)
      tmpMaterial.diffuseTex = basePath + strtok(nullptr, "\t\n\r");
    else if(strcmp(token, "map_Ks") == 0)
      tmpMaterial.specularTex = basePath + strtok(nullptr, "\t\n\r");
    else if(strcmp(token, "bump") == 0)
      tmpMaterial.bumpMap = basePath + strtok(nullptr, "\t\n\r");
    else if(strcmp(token, "illum") == 0)
      tmpMaterial.illum = atoi(strtok(nullptr, " \t\n\r"));
    else if(strcmp(token, "Ns") == 0)
      tmpMaterial.specularExponent = (float)atof(strtok(nullptr, " \t\n\r"));
    else if(strcmp(token, "Ni") == 0)
      tmpMaterial.refractionIndex = (float)atof(strtok(nullptr, " \t\n\r"));
    else if(strcmp(token, "Tf") == 0)
    {
      glm::vec3 c(glm::vec3::_null);
      c.x = (float)atof(strtok(nullptr, " \t\n\r"));
      c.y = (float)atof(strtok(nullptr, " \t\n\r"));
      c.z = (float)atof(strtok(nullptr, " \t\n\r"));
      tmpMaterial.transmissionFilter = c;
    }
    else if(strcmp(token, "d") == 0 || strcmp(token, "Tr") == 0)
      tmpMaterial.transparency = (float)atof(strtok(nullptr, " \t\n\r"));
    else if(strcmp(token, "sharpness") == 0)
      tmpMaterial.sharpness = (float)atof(strtok(nullptr, " \t\n\r"));
    else
    {
      printf("Unhandled token in material: [%s]\n", token);
    }
  }
  if(inMaterial) //add the last material
    out_materials.push_back(tmpMaterial);

  return 0;
}

int LoadOBJ(const std::string &filename,
            std::vector<std::vector<Vertex>> &vertexArrays,
            std::vector<std::vector<uint32_t>> &indexArrays,
            std::vector<Texture*> &textures,
            std::vector<glm::vec3> &colors)
{
  FILE *file = fopen((filename + ".cached").c_str(), "rb");
  if(file)
  {
    //read cached
    uint32_t materialCount;
    fread(&materialCount, sizeof(uint32_t), 1, file);

    std::vector<CachedMaterial> cachedMaterials;
    cachedMaterials.resize(materialCount);

    fread(cachedMaterials.data(), sizeof(CachedMaterial), materialCount, file);

    uint32_t meshCount;
    fread(&meshCount, sizeof(uint32_t), 1, file);

    vertexArrays.resize(meshCount);
    indexArrays.resize(meshCount);

    textures.reserve(meshCount);
    colors.reserve(meshCount);

    for(uint i = 0; i < meshCount; ++i)
    {
      std::vector<Vertex> &vertices = vertexArrays[i];
      std::vector<uint32_t> &indices = indexArrays[i];

      uint32_t vertexCount, indexCount;
      fread(&vertexCount, sizeof(uint32_t), 1, file);
      fread(&indexCount, sizeof(uint32_t), 1, file);

      vertices.resize(vertexCount);
      indices.resize(indexCount);

      fread(vertices.data(), sizeof(Vertex), vertexCount, file);
      fread(indices.data(), sizeof(uint32_t), indexCount, file);

      uint32_t materialIdx;
      fread(&materialIdx, sizeof(uint32_t), 1, file);

      const CachedMaterial& material = cachedMaterials[materialIdx];

      Texture* tex = TextureManager::GI().GetTexture(material.diffuseTex);
      textures.push_back(tex);

      colors.push_back(material.diffuseColor);
    }

    fclose(file);
    return 0;
  }


  file = fopen(filename.c_str(), "rb");
  if(file == nullptr)
    return 1;

  //automatically close the file when returning from the function
  std::shared_ptr<FILE> safe_file(file, &fclose);

  std::string path = ResolvePath(filename);

  char line[0x100];
  std::vector<glm::vec3> tmpPositions;
  std::vector<glm::vec3> tmpNormals;
  std::vector<glm::vec2> tmpTexcoords;
  tmpPositions.reserve(0x20000);
  tmpNormals.reserve(0x20000);
  tmpTexcoords.reserve(0x20000);
  tmpPositions.push_back(glm::vec3(0, 0, 0));
  tmpNormals.push_back(glm::vec3(0, 0, 0));
  tmpTexcoords.push_back(glm::vec2(0, 0));

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(0x20000);
  indices.reserve(0x20000);
  bool inMesh = false;

  std::vector<OBJMaterial> materials;
  int materialIdx = 0;
  std::vector<uint32_t> materialIndices;

  uint v[16] = {0}, n[16] = {0}, t[16] = {0};
  for(int i = 1; fgets(line, sizeof(line), file) != nullptr; ++i)
  {
    size_t len = strlen(line);
    if(len == 0) continue;
    if(line[len - 1] == '\n')
    {
      line[len - 1] = '\0';
      len--;
    }
    if(len == 0) continue;
    if(line[0] == '#' || line[0] == '\n') continue;

    switch(line[0])
    {
    case 'v':
      if(line[1] == 'n')
      {
        const char* ptr = strtok(line + 3, " \t\n\r");
        glm::vec3 norm(glm::vec3::_null);
        norm.x = (float)atof(ptr);
        norm.y = (float)atof(strtok(nullptr, " \t\n\r"));
        norm.z = (float)atof(strtok(nullptr, " \t\n\r"));
        tmpNormals.push_back(norm);
      }
      else if(line[1] == 't')
      {
        const char* ptr = strtok(line + 3, " \t\n\r");
        glm::vec2 tc(glm::vec2::_null);
        tc.x = (float)atof(ptr);
        tc.y = (float)atof(strtok(nullptr, " \t\n\r"));

        //sscanf(line + 3, "%f %f", &tc.x, &tc.y);
        tmpTexcoords.push_back(tc);
      }
      else
      {
        const char* ptr = strtok(line + 2, " \t\n\r");
        glm::vec3 pos(glm::vec3::_null);
        pos.x = (float)atof(ptr);
        pos.y = (float)atof(strtok(nullptr, " \t\n\r"));
        pos.z = (float)atof(strtok(nullptr, " \t\n\r"));

        //sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z);
        tmpPositions.push_back(pos);
      }
      break;
    case 'f':
    {
      uint verts = ParseFace(line + 2, v, t, n);
      Vertex vert0;
      vert0.position = tmpPositions[v[0]];
      vert0.normal = tmpNormals[n[0]];
      vert0.texcoord = tmpTexcoords[t[0]];
      v[0] = n[0] = t[0] = 0;

      Vertex vert1;
      vert1.position = tmpPositions[v[1]];
      vert1.normal = tmpNormals[n[1]];
      vert1.texcoord = tmpTexcoords[t[1]];
      v[1] = n[1] = t[1] = 0;

      for(uint j = 2; j < verts; ++j)
      {
        Vertex vert2;
        vert2.position = tmpPositions[v[j]];
        vert2.normal = tmpNormals[n[j]];
        vert2.texcoord = tmpTexcoords[t[j]];
        v[j] = n[j] = t[j] = 0;

        vertices.push_back(vert0);
        vertices.push_back(vert1);
        vertices.push_back(vert2);

        vert1 = vert2;
      }
      break;
    }
    case 'm':
      if(strncmp(line, "mtllib", 6) == 0)
      {
        char name[128];
        name[0] = '\0';
        strcat(name, path.c_str());
        strcat(name, line + 7);
        LoadMaterials(path, name, materials);
      }
      break;
    case 'u':
      if(strncmp(line, "usemtl", 6) == 0)
      {
        if(inMesh)
        {
          IndexVertices(vertices, indices);

          bool appended = false;
          for(size_t i = 0; i < materialIndices.size(); ++i)
          {
            if(materialIndices[i] != (uint32_t)materialIdx)
              continue;

            indexArrays[i].reserve(indexArrays[i].size() + indices.size());
            for(size_t j = 0; j < indices.size(); ++j)
            {
              uint32_t index = uint32_t(indices[j] + vertexArrays[i].size());
              indexArrays[i].push_back(index);
            }
            vertexArrays[i].reserve(vertexArrays[i].size() + vertices.size());
            for(size_t j = 0; j < vertices.size(); ++j)
              vertexArrays[i].push_back(vertices[j]);

            appended = true;
            break;
          }

          if(!appended)
          {
            const std::string &texName = materials[materialIdx].diffuseTex;
            Texture *tex = nullptr;
            if(texName != "")
              tex = TextureManager::GI().GetTexture(texName);
            const glm::vec3 diffuseColor = materials[materialIdx].diffuseColor;

            vertexArrays.push_back(std::move(vertices));
            indexArrays.push_back(std::move(indices));
            textures.push_back(tex);
            colors.push_back(diffuseColor);

            materialIndices.push_back((uint32_t)materialIdx);
          }

          vertices.clear();
          indices.clear();

        }
        inMesh = true;

        //use new material
        const char* materialName = line + 7;
        int c = 0;
        for(auto &m : materials)
        {
          if(m.name == materialName)
          {
            materialIdx = c;
            break;
          }
          ++c;
        }
      }
      break;
    case 's':
    case 'g':
      break;
    default:
      printf("Don't know what do to with this line: %d:[%s]\n", i, line);
      break;
    }
  }

  if(inMesh)
  {
    IndexVertices(vertices, indices);
    vertexArrays.push_back(std::move(vertices));
    indexArrays.push_back(std::move(indices));

    const std::string &texName = materials[materialIdx].diffuseTex;
    Texture *tex = nullptr;
    if(texName != "")
      tex = TextureManager::GI().GetTexture(texName);
    textures.push_back(tex);

    colors.push_back(materials[materialIdx].diffuseColor);

    materialIndices.push_back((uint32_t)materialIdx);

    vertices.clear();
    indices.clear();
  }

  fclose(file);

  file = fopen((filename + ".cached").c_str(), "wb");
  if(file)
  {
    //read cached
    uint32_t materialCount = (uint32_t)materials.size();
    fwrite(&materialCount, sizeof(uint32_t), 1, file);

    std::vector<CachedMaterial> cachedMaterials;
    cachedMaterials.reserve(materialCount);

    for(auto &m : materials)
      cachedMaterials.push_back(CachedMaterial(m));

    fwrite(cachedMaterials.data(), sizeof(CachedMaterial), materialCount, file);

    uint32_t meshCount = (uint32_t)vertexArrays.size();
    fwrite(&meshCount, sizeof(uint32_t), 1, file);

    for(uint i = 0; i < meshCount; ++i)
    {
      const std::vector<Vertex> &vertices = vertexArrays[i];
      const std::vector<uint32_t> &indices = indexArrays[i];

      const uint32_t vertexCount = (uint32_t)vertices.size();
      const uint32_t indexCount = (uint32_t)indices.size();
      fwrite(&vertexCount, sizeof(uint32_t), 1, file);
      fwrite(&indexCount, sizeof(uint32_t), 1, file);


      fwrite(vertices.data(), sizeof(Vertex), vertexCount, file);
      fwrite(indices.data(), sizeof(uint32_t), indexCount, file);

      const uint32_t materialIdx = (uint32_t)materialIndices[i];
      fwrite(&materialIdx, sizeof(uint32_t), 1, file);
    }

    fclose(file);
  }
  return 0;
}

void LoadOBJ(const std::string &filename,
             std::vector<Vertex> &vertices,
             std::vector<uint32_t> &indices)
{
  FILE *file = fopen((filename + ".cached").c_str(), "rb");
  if(file)
  {
    //read cached
    uint32_t vertexCount;
    uint32_t indexCount;
    fread(&vertexCount, sizeof(uint32_t), 1, file);
    fread(&indexCount, sizeof(uint32_t), 1, file);

    vertices.resize(vertexCount);
    indices.resize(indexCount);

    fread(vertices.data(), sizeof(Vertex), vertexCount, file);
    fread(indices.data(), sizeof(uint32_t), indexCount, file);

    fclose(file);
    return;
  }

  std::vector<glm::vec3> tmpPositions;
  std::vector<glm::vec3> tmpNormals;
  std::vector<glm::vec2> tmpTexcoords;
  std::vector<int> tmpPIndices, tmpNInices, tmpTIndices;

  LoadOBJ(filename,
          tmpPositions, tmpNormals, tmpTexcoords,
          tmpPIndices, tmpNInices, tmpTIndices);

  vertices.reserve(tmpPIndices.size());
  for(size_t i = 0; i < tmpPIndices.size(); ++i)
  {
    Vertex v;
    v.position = tmpPositions[tmpPIndices[i]];
    v.normal = tmpNormals[tmpNInices[i]];
    v.texcoord = tmpTexcoords[tmpTIndices[i]];
    vertices.push_back(v);
  }


  IndexVertices(vertices, indices);
  vertices.shrink_to_fit();

  file = fopen((filename + ".cached").c_str(), "wb");
  const uint32_t vertexCount = (uint32_t)vertices.size();
  const uint32_t indexCount = (uint32_t)indices.size();
  fwrite(&vertexCount, sizeof(uint32_t), 1, file);
  fwrite(&indexCount, sizeof(uint32_t), 1, file);

  fwrite(vertices.data(), sizeof(Vertex), vertices.size(), file);
  fwrite(indices.data(), sizeof(uint32_t), indices.size(), file);

  fclose(file);
}
