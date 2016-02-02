#include <cassert>

#include <queue>

#include <immintrin.h>

#include "octree.h"
#include "timer.h"
#include "octree.h"
#include "morton.h"

#define PIXEL_R(x) (((x) >> 16) & 0xff)
#define PIXEL_G(x) (((x) >>  8) & 0xff)
#define PIXEL_B(x) ((x) & 0xff)

void Octree::Init(const uint32_t* denseVoxels, int sizeX, int sizeY, int sizeZ)
{
  m_sizeX = sizeX, m_sizeY = sizeY, m_sizeZ = sizeZ;

  Timer timer;
  timer.Start();

  assert(sizeX == sizeY && sizeY == sizeZ);

  int size = sizeX * sizeY * sizeZ;
  std::vector<Node> tmpNodeBuffer;
  tmpNodeBuffer.reserve(size / 8);
  Node node = {0};
  node.child = 0;

  std::vector<int32_t> tmpBuffer0;
  std::vector<int32_t> tmpBuffer1;
  tmpBuffer0.reserve(size / 8);
  tmpBuffer1.reserve(size / 64);
  std::vector<int32_t>* buffer[2] = {&tmpBuffer0, &tmpBuffer1};

  for(int i = 0; i < size; i += 8)
  {
    Node parent;
    uint32_t r = 0, g = 0, b = 0, c = 0;

    bool any = false;
    for(int j = 0; j < 8; ++j) if(denseVoxels[i + j]) { any = true; break; }

    if(any)
    {
      parent.child = (int32_t)m_nodes.size();

      for(int j = 0; j < 8; ++j)
      {
        const uint32_t voxel = denseVoxels[i + j];
        if(voxel)
        {
          any = true;
          r += PIXEL_R(voxel);
          g += PIXEL_G(voxel);
          b += PIXEL_B(voxel);
          c++;

          node.color = voxel;
          m_nodes.push_back(node);
        }
        else
        {
          node.color = 0;
          m_nodes.push_back(node);
        }
      }

      r /= c, g /= c, b /= c;
      parent.color = (r << 16) | (g << 8) | b;
      tmpNodeBuffer.push_back(parent);
      tmpBuffer0.push_back((int32_t)tmpNodeBuffer.size() - 1);
    }
    else
      tmpBuffer0.push_back(-1);
  }


  //fix all the indices
  for(size_t i = 0; i < tmpBuffer0.size(); ++i)
    if(tmpBuffer0[i] != -1) tmpBuffer0[i] += (int32_t)m_nodes.size();

  //copy all the temporary nodes in the nodes buffer
  m_nodes.reserve(m_nodes.size() + tmpNodeBuffer.size());
  for(int i = 0; i < tmpNodeBuffer.size(); ++i)
    m_nodes.push_back(tmpNodeBuffer[i]);

  //keep creating parent nodes until there is only a single node left
  while(buffer[0]->size() > 1)
  {
    for(size_t i = 0; i < buffer[0]->size(); i += 8)
    {
      //accumulators for calculating the average color
      //c is a counter, counting the number of accumulated colors
      uint32_t r = 0, g = 0, b = 0, c = 0;

      //flag to determine if there are any childeren
      bool any = false;
      for(int j = 0; j < 8; ++j) if((*buffer[0])[i + j] != -1) { any = true; break; }

      node.child = (*buffer[0])[i];

      //store the index of the new parent node, if no new node is created we
      // store -1
      int32_t idxVal;

      //if there are childeren, create a parent node
      if(any)
      {
        for(int j = 0; j < 8; ++j)
        {
          const int32_t childIdx = (*buffer[0])[i + j];
          /*if there is a child*/
          if(childIdx != -1)
          {
            /*accumulate for average color*/
            r += PIXEL_R(m_nodes[childIdx].color);
            g += PIXEL_G(m_nodes[childIdx].color);
            b += PIXEL_B(m_nodes[childIdx].color);
            c++;
          }
        }


          //calculate average color 
        r /= c, g /= c, b /= c;
        node.color = (r << 16) | (g << 8) | b;

        //ad node
        m_nodes.push_back(node);

        //generate new index value
        idxVal = (int32_t)m_nodes.size() - 1;
      }
      else //otherwise we don't want a new node
        idxVal = -1;

      //add the index of the (optionally) newly created node to the output buffer
      buffer[1]->push_back(idxVal);
    }

    //clear the input buffer
    buffer[0]->clear();

    //swap in and output
    std::swap(buffer[0], buffer[1]);
  }

  double timeMS = timer.IntervalMS();
  printf("constructed octree in %.2f ms.\n", timeMS);
}

void Octree::FillPosAndColorBuffer_R(
  const Node* node, int x, int y, int z, int level, int targetLevel,
  std::vector<glm::vec3>& posBuffer, std::vector<uint32_t> &colorBuffer) const
{
  if(level == targetLevel)
  {
    if(node->color != 0)
    {
      posBuffer.push_back(glm::vec3(x, y, z));
      colorBuffer.push_back(node->color);
    }
    return;
  }

  const int sizeX = m_sizeX / (1 << level);//(int)pow(2, level);
  const int sizeY = m_sizeY / (1 << level);//(int)pow(2, level);
  const int sizeZ = m_sizeZ / (1 << level);//(int)pow(2, level);

  const int halfSizeX = sizeX / 2;
  const int halfSizeY = sizeY / 2;
  const int halfSizeZ = sizeZ / 2;

  if(node->child == 0)
    return;

  for(int j = 0; j < 8; ++j)
  {
    //skip empty nodes

    const int offsetX = ((j >> 0 & 1) ? halfSizeX : -halfSizeX) >> 1;
    const int offsetY = ((j >> 1 & 1) ? halfSizeX : -halfSizeX) >> 1;
    const int offsetZ = ((j >> 2 & 1) ? halfSizeX : -halfSizeX) >> 1;

    const int cx = x + offsetX;
    const int cy = y + offsetY;
    const int cz = z + offsetZ;

    FillPosAndColorBuffer_R(
      &m_nodes[node->child + j], cx, cy, cz, level + 1,
      targetLevel, posBuffer, colorBuffer);
  }
}

void Octree::FillPosAndColorBuffer(int targetLevel,
                                   std::vector<glm::vec3>& posBuffer, std::vector<uint32_t> &colorBuffer) const
{
  if(m_nodes.size() == 0)
    return;
  FillPosAndColorBuffer_R(
    &m_nodes[0],
    m_sizeX / 2, m_sizeY / 2, m_sizeZ / 2, 0,
    targetLevel, posBuffer, colorBuffer);
}

void Octree::CheckTree()
{
  int stack[128];
  int stackIdx = 0;

  int nodeIdx = 0;

  Node* root = m_nodes.data();

  for(;;)
  {
    root[nodeIdx].color |= 0xff000000;

    if(root[nodeIdx].child != 0)
    {
      for(int i = 1; i < 8; ++i)
        stack[stackIdx++] = root[nodeIdx].child + i;

      nodeIdx = root[nodeIdx].child;
    }
    else
    {
      if(stackIdx == 0)
        break;

      nodeIdx = stack[--stackIdx];
    }
  }

  int unreachableNodes = 0;
  for(auto& n : m_nodes)
  {
    if((n.color & 0xff000000) == 0)
      unreachableNodes++;
    n.color &= 0x00ffffff;
  }

  if(unreachableNodes > 0)
    printf("%d unreachable nodes found!\n", unreachableNodes);


  for(auto &n : m_nodes)
  {
    if(n.child >= m_nodes.size())
      printf("node %zu is pointing to an invalid child!\n", &n - m_nodes.data());
  }

}
