#line 1 "generate_mipmap.c.glsl"
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

uniform layout(r32ui, binding = 0) restrict uimage3D u_diffuseColorImage;
uniform layout(r32ui, binding = 1) restrict uimage3D u_normalImage;
uniform layout(r32ui, binding = 2) restrict uimage3D u_lightImage;

layout(std430, binding=0) restrict buffer Octree { OctreeNode nodes[]; } octree;
uniform layout(location = 0) uint u_layer;

#define SLOW


uint Traverse(ivec3 ipos, uint layer)
{
  uint nodeIdx = 0u;
  for(uint depth = 0u; depth < layer; ++depth)
  {
    //if this node has no children, stop traversal
    if(octree.nodes[nodeIdx].child == 0u)
      return 0u;
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1) << 0 ) | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
    
    //traverse to the child node in the direction specified
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  return nodeIdx;
}

void CalculateNeighbours(uint nodeIdx, ivec3 ipos)
{
  uint child = octree.nodes[nodeIdx].child;
  
  uint parXPos = Traverse(ipos + (ivec3(1, 0, 0) << (MAX_DEPTH - u_layer)), u_layer);
  uint parYPos = Traverse(ipos + (ivec3(0, 1, 0) << (MAX_DEPTH - u_layer)), u_layer);
  uint parZPos = Traverse(ipos + (ivec3(0, 0, 1) << (MAX_DEPTH - u_layer)), u_layer);
  
  if(parXPos != 0u && octree.nodes[parXPos].child != 0u)
  {
    octree.nodes[child + 1].neighbourXPos = octree.nodes[parXPos].child + 0;
    octree.nodes[child + 3].neighbourXPos = octree.nodes[parXPos].child + 2;
    octree.nodes[child + 5].neighbourXPos = octree.nodes[parXPos].child + 4;
    octree.nodes[child + 7].neighbourXPos = octree.nodes[parXPos].child + 6;
  
    octree.nodes[octree.nodes[parXPos].child + 0].neighbourXNeg = child + 1;
    octree.nodes[octree.nodes[parXPos].child + 2].neighbourXNeg = child + 3;
    octree.nodes[octree.nodes[parXPos].child + 4].neighbourXNeg = child + 5;
    octree.nodes[octree.nodes[parXPos].child + 6].neighbourXNeg = child + 7;
  }
  else
  {
    octree.nodes[child + 1].neighbourXPos = 0u;
    octree.nodes[child + 3].neighbourXPos = 0u;
    octree.nodes[child + 5].neighbourXPos = 0u;
    octree.nodes[child + 7].neighbourXPos = 0u;
    //if we are going a partially static tree, we will have to set these pointer to 0
    //otherwise they are already set to 0 by the construction
  }
  
  if(parYPos != 0u && octree.nodes[parYPos].child != 0u)
  {
    octree.nodes[child + 2].neighbourYPos = octree.nodes[parYPos].child + 0;
    octree.nodes[child + 3].neighbourYPos = octree.nodes[parYPos].child + 1;
    octree.nodes[child + 6].neighbourYPos = octree.nodes[parYPos].child + 4;
    octree.nodes[child + 7].neighbourYPos = octree.nodes[parYPos].child + 5;
    
    octree.nodes[octree.nodes[parYPos].child + 0].neighbourYNeg = child + 2;
    octree.nodes[octree.nodes[parYPos].child + 1].neighbourYNeg = child + 3;
    octree.nodes[octree.nodes[parYPos].child + 4].neighbourYNeg = child + 6;
    octree.nodes[octree.nodes[parYPos].child + 5].neighbourYNeg = child + 7;
  }
  else
  {
    octree.nodes[child + 2].neighbourYPos = 0u;
    octree.nodes[child + 3].neighbourYPos = 0u;
    octree.nodes[child + 6].neighbourYPos = 0u;
    octree.nodes[child + 7].neighbourYPos = 0u;
    //if we are going a partially static tree, we will have to set these pointer to 0
    //otherwise they are already set to 0 by the construction
  }
  
  if(parZPos != 0u && octree.nodes[parZPos].child != 0u)
  {
    octree.nodes[child + 4].neighbourZPos = octree.nodes[parZPos].child + 0;
    octree.nodes[child + 5].neighbourZPos = octree.nodes[parZPos].child + 1;
    octree.nodes[child + 6].neighbourZPos = octree.nodes[parZPos].child + 2;
    octree.nodes[child + 7].neighbourZPos = octree.nodes[parZPos].child + 3;
    
    octree.nodes[octree.nodes[parZPos].child + 0].neighbourZNeg = child + 4;
    octree.nodes[octree.nodes[parZPos].child + 1].neighbourZNeg = child + 5;
    octree.nodes[octree.nodes[parZPos].child + 2].neighbourZNeg = child + 6;
    octree.nodes[octree.nodes[parZPos].child + 3].neighbourZNeg = child + 7;
  }
  else
  {
    octree.nodes[child + 4].neighbourZPos = 0u;
    octree.nodes[child + 5].neighbourZPos = 0u;
    octree.nodes[child + 6].neighbourZPos = 0u;
    octree.nodes[child + 7].neighbourZPos = 0u;
    //if we are going a partially static tree, we will have to set these pointer to 0
    //otherwise they are already set to 0 by the construction
  }
}

uint MaskAlpha(uint a)
{
  return (a >> 24) != 0 ? (a | 0xff000000) : 0;
}

void DuplicateRight(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a, b, c, d;
  uint na, nb, nc, nd;
  uint la, lb, lc, ld;
  a = b = c = d = 0u;
  na = nb = nc = nd = 0u;
  la = lb = lc = ld = 0u;
  //a = b = c = d = 0xffffffff;
  
  #if defined(SLOW)
  ivec3 ipos = dstIpos + (ivec3(2, 0, 0) << shift);
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) == 0)
  {  
    //a = b = c = d = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {
      //a = b = c = d = uvec4(0xff00ff00);
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {
        //a = b = c = d = uvec4(0xff0000ff);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 0)).r;
        c = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 1)).r;
        d = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 1)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 0)).r;
        nc = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 1)).r;
        nd = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 1)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 0)).r;
        lc = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 1)).r;
        ld = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 1)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          c = MaskAlpha(c);
          d = MaskAlpha(d);
          
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
          nc = MaskAlpha(nc);
          nd = MaskAlpha(nd);
        }
      }
    }
  }
  #else
    uint neighbour = octree.nodes[thisNode].neighbourXPos;
    if(neighbour != 0u)
    {
      //a = b = c = d = uvec4(0xffff0000);
      uint brick = octree.nodes[neighbour].brick;
      if(brick != 0u)
      {
        //a = b = c = d = uvec4(0xff00ff00);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 0)).r;
        c = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 1)).r;
        d = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 1)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 0)).r;
        nc = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 1)).r;
        nd = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 1)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 0)).r;
        lc = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 1)).r;
        ld = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 1)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          c = MaskAlpha(c);
          d = MaskAlpha(d);
          
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
          nc = MaskAlpha(nc);
          nd = MaskAlpha(nd);
        }
      }
    }
  
  #endif
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 0, 0), uvec4(a, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 1, 0), uvec4(b, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 0, 1), uvec4(c, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 1, 1), uvec4(d, 0, 0, 0));
  
  imageStore(u_normalImage, dstImagePos + ivec3(2, 0, 0), uvec4(na, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(2, 1, 0), uvec4(nb, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(2, 0, 1), uvec4(nc, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(2, 1, 1), uvec4(nd, 0, 0, 0));
  
  imageStore(u_lightImage, dstImagePos + ivec3(2, 0, 0), uvec4(la, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(2, 1, 0), uvec4(lb, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(2, 0, 1), uvec4(lc, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(2, 1, 1), uvec4(ld, 0, 0, 0));
}

void DuplicateTop(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a, b, c, d;
  uint na, nb, nc, nd;
  uint la, lb, lc, ld;
  a = b = c = d = 0u;
  na = nb = nc = nd = 0u;
  la = lb = lc = ld = 0u;
  //a = b = c = d = uvec4(0xffffffff);
  
  #if defined(SLOW)
  ivec3 ipos = dstIpos + (ivec3(0, 2, 0) << shift);
  if((ipos.y & RESOLUTION_MASK) == 0)
  {  
    //a = b = c = d = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {      
        //a = b = c = d = uvec4(0xff0000ff);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 0)).r;
        c = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 1)).r;
        d = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 1)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 0)).r;
        nc = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 1)).r;
        nd = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 1)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 0)).r;
        lc = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 1)).r;
        ld = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 1)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          c = MaskAlpha(c);
          d = MaskAlpha(d);
          
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
          nc = MaskAlpha(nc);
          nd = MaskAlpha(nd);
        }
      }      
    }
  }
  #else
  uint neighbour = octree.nodes[thisNode].neighbourYPos;
  if(neighbour != 0u)
  {
    uint brick = octree.nodes[neighbour].brick;
    if(brick != 0u)
    {
      ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
      a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
      b = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 0)).r;
      c = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 1)).r;
      d = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 1)).r;
        
      na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
      nb = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 0)).r;
      nc = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 1)).r;
      nd = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 1)).r;
        
      la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
      lb = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 0)).r;
      lc = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 1)).r;
      ld = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 1)).r;
      
      if(u_layer == MAX_DEPTH - 2)
      {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          c = MaskAlpha(c);
          d = MaskAlpha(d);
          
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
          nc = MaskAlpha(nc);
          nd = MaskAlpha(nd);
      }
    }
  }
  #endif
  
  //imageStore(u_diffuseColorImage, dstImagePos + ivec3(0, 2, 0), a);
  //imageStore(u_diffuseColorImage, dstImagePos + ivec3(1, 2, 0), b);
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(0, 2, 0), uvec4(a, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(1, 2, 0), uvec4(b, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(0, 2, 1), uvec4(c, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(1, 2, 1), uvec4(d, 0, 0, 0));
  
  imageStore(u_normalImage, dstImagePos + ivec3(0, 2, 0), uvec4(na, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(1, 2, 0), uvec4(nb, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(0, 2, 1), uvec4(nc, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(1, 2, 1), uvec4(nd, 0, 0, 0));
  
  imageStore(u_lightImage, dstImagePos + ivec3(0, 2, 0), uvec4(la, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(1, 2, 0), uvec4(lb, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(0, 2, 1), uvec4(lc, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(1, 2, 1), uvec4(ld, 0, 0, 0));
}

void DuplicateBack(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a, b, c, d;
  uint na, nb, nc, nd;
  uint la, lb, lc, ld;
  a = b = c = d = 0u;
  na = nb = nc = nd = 0u;
  la = lb = lc = ld = 0u;
  //a = b = c = d = uvec4(0xffffffff);
  #if defined(SLOW)
  ivec3 ipos = dstIpos + (ivec3(0, 0, 2) << shift);
  if((ipos.z & RESOLUTION_MASK) == 0)
  {
    //a = b = c = d = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {
      //a = b = c = d = uvec4(0xff00ff00);
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {
        //a = b = c = d = uvec4(0xff0000ff);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 0)).r;
        c = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 0)).r;
        d = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 1, 0)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 0)).r;
        nc = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 0)).r;
        nd = imageLoad(u_normalImage, imagePos + ivec3(1, 1, 0)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 0)).r;
        lc = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 0)).r;
        ld = imageLoad(u_lightImage, imagePos + ivec3(1, 1, 0)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          c = MaskAlpha(c);
          d = MaskAlpha(d);
          
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
          nc = MaskAlpha(nc);
          nd = MaskAlpha(nd);
        }
      }
    }
  }
  #else
  uint neighbour = octree.nodes[thisNode].neighbourZPos;
  if(neighbour != 0u)
  {
    //a = b = c = d = uvec4(0xffff0000);
    uint brick = octree.nodes[neighbour].brick;
    if(brick != 0u)
    {
      //a = b = c = d = uvec4(0xff00ff00);
      ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
      a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
      b = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 0)).r;
      c = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 0)).r;
      d = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 1, 0)).r;
      
      na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
      nb = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 0)).r;
      nc = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 0)).r;
      nd = imageLoad(u_normalImage, imagePos + ivec3(1, 1, 0)).r;
        
      la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
      lb = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 0)).r;
      lc = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 0)).r;
      ld = imageLoad(u_lightImage, imagePos + ivec3(1, 1, 0)).r;
      
      if(u_layer == MAX_DEPTH - 2)
      {
        a = MaskAlpha(a);
        b = MaskAlpha(b);
        c = MaskAlpha(c);
        d = MaskAlpha(d);

        na = MaskAlpha(na);
        nb = MaskAlpha(nb);
        nc = MaskAlpha(nc);
        nd = MaskAlpha(nd);
      }
    }
  }
  #endif
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(0, 0, 2), uvec4(a, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(1, 0, 2), uvec4(b, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(0, 1, 2), uvec4(c, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(1, 1, 2), uvec4(d, 0, 0, 0));
  
  imageStore(u_normalImage, dstImagePos + ivec3(0, 0, 2), uvec4(na, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(1, 0, 2), uvec4(nb, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(0, 1, 2), uvec4(nc, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(1, 1, 2), uvec4(nd, 0, 0, 0));
  
  imageStore(u_lightImage, dstImagePos + ivec3(0, 0, 2), uvec4(la, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(1, 0, 2), uvec4(lb, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(0, 1, 2), uvec4(lc, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(1, 1, 2), uvec4(ld, 0, 0, 0));
}

void DuplicateTopBack(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a, b, na, nb, la, lb;
  a = b = 0;
  na = nb = 0;
  la = lb = 0;
  //a = b = uvec4(0xffffffff);
  ivec3 ipos = dstIpos + (ivec3(0, 2, 2) << shift);
  if(((ipos.y | ipos.z) & RESOLUTION_MASK) == 0)
  {  
    //a = b = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {  
      //a = b = uvec4(0xff00ff00);
    
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {
        //a = b = uvec4(0xff0000ff);
      
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(1, 0, 0)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(1, 0, 0)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(1, 0, 0)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
        }
      }
    }
  }
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(0, 2, 2), uvec4(a, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(1, 2, 2), uvec4(b, 0, 0, 0));
  
  imageStore(u_normalImage, dstImagePos + ivec3(0, 2, 2), uvec4(na, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(1, 2, 2), uvec4(nb, 0, 0, 0));
  
  imageStore(u_lightImage, dstImagePos + ivec3(0, 2, 2), uvec4(la, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(1, 2, 2), uvec4(lb, 0, 0, 0));
}

void DuplicateRightBack(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a, b, na, nb, la, lb;
  a = b = 0;
  na = nb = 0;
  la = lb = 0;
  //a = b = uvec4(0xffffffff);
  ivec3 ipos = dstIpos + (ivec3(2, 0, 2) << shift);
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) == 0)
  {
    //a = b = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {
      //a = b = uvec4(0xff00ff00);
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {
        //a = b = uvec4(0xff0000ff);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 1, 0)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(0, 1, 0)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(0, 1, 0)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
        }
      }
    }
  }
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 0, 2), uvec4(a, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 1, 2), uvec4(b, 0, 0, 0));
  
  imageStore(u_normalImage, dstImagePos + ivec3(2, 0, 2), uvec4(na, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(2, 1, 2), uvec4(nb, 0, 0, 0));
  
  imageStore(u_lightImage, dstImagePos + ivec3(2, 0, 2), uvec4(la, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(2, 1, 2), uvec4(lb, 0, 0, 0));
}

void DuplicateTopRight(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a, b, na, nb, la, lb;
  a = b = 0;
  na = nb = 0;
  la = lb = 0;
  //a = b = uvec4(0xffffffff);
  ivec3 ipos = dstIpos + (ivec3(2, 2, 0) << shift);
  if(((ipos.x | ipos.y) & RESOLUTION_MASK) == 0)
  {
    //a = b = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {
      //a = b = uvec4(0xff00ff00);
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {
        //a = b = uvec4(0xff0000ff);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 0)).r;
        b = imageLoad(u_diffuseColorImage, imagePos + ivec3(0, 0, 1)).r;
        
        na = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 0)).r;
        nb = imageLoad(u_normalImage, imagePos + ivec3(0, 0, 1)).r;
        
        la = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 0)).r;
        lb = imageLoad(u_lightImage, imagePos + ivec3(0, 0, 1)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          b = MaskAlpha(b);
          
          na = MaskAlpha(na);
          nb = MaskAlpha(nb);
        }
      }
    }
  }
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 2, 0), uvec4(a, 0, 0, 0));
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 2, 1), uvec4(b, 0, 0, 0));
  
  imageStore(u_normalImage, dstImagePos + ivec3(2, 2, 0), uvec4(na, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(2, 2, 1), uvec4(nb, 0, 0, 0));
  
  imageStore(u_lightImage, dstImagePos + ivec3(2, 2, 0), uvec4(la, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(2, 2, 1), uvec4(lb, 0, 0, 0));
}

void DuplicateCorner(uint thisNode, ivec3 dstIpos, ivec3 dstImagePos, uint shift)
{
  uint a = 0u;
  uint na = 0u;
  uint la = 0u;
  //a = uvec4(0xffffffff);
  ivec3 ipos = dstIpos + (ivec3(2, 2, 2) << shift);
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) == 0)
  {
    //a = uvec4(0xffff0000);
    uint nodeIdx = Traverse(ipos, u_layer + 1);
    if(nodeIdx != 0u)
    {
      //a = uvec4(0xff00ff00);
      uint brick = octree.nodes[nodeIdx].brick;
      if(brick != 0u)
      {
        //a = uvec4(0xff0000ff);
        ivec3 imagePos = BrickToImagePos(brick, imageSize(u_diffuseColorImage));
        
        a = imageLoad(u_diffuseColorImage, imagePos + ivec3(0,0,0)).r;        
        na = imageLoad(u_normalImage, imagePos + ivec3(0,0,0)).r;
        la = imageLoad(u_lightImage, imagePos + ivec3(0,0,0)).r;
        
        if(u_layer == MAX_DEPTH - 2)
        {
          a = MaskAlpha(a);
          na = MaskAlpha(na);
        }
      }
    }
  }
  imageStore(u_diffuseColorImage, dstImagePos + ivec3(2, 2, 2), uvec4(a, 0, 0, 0));
  imageStore(u_normalImage, dstImagePos + ivec3(2, 2, 2), uvec4(na, 0, 0, 0));
  imageStore(u_lightImage, dstImagePos + ivec3(2, 2, 2), uvec4(la, 0, 0, 0));
}

void main()
{
  int scale = int(MAX_DEPTH - u_layer);
  ivec3 ipos = ivec3(
    gl_GlobalInvocationID.x << scale,
    gl_GlobalInvocationID.y << scale,
    gl_GlobalInvocationID.z << scale);
  
  if(((ipos.x | ipos.y | ipos.z) & RESOLUTION_MASK) != 0)
    return;
  
  uint nodeIdx = 0u; //start at the root
  for(uint depth = 0u; depth < u_layer; ++depth)
  {
    //if this node has no children, stop traversal
    if(octree.nodes[nodeIdx].child == 0)
      return;
    
    //calculate the direction to go to
    uint shift = (MAX_DEPTH - 1) - depth;
    uint direction = 
      ( ((ipos.x >> shift) & 1))  | 
      ( ((ipos.y >> shift) & 1) << 1 ) |
      ( ((ipos.z >> shift) & 1) << 2 );
      
    //traverse to the child node in the direction specified
    nodeIdx = octree.nodes[nodeIdx].child + direction;
  }
  
  //nothing to blend
  if(octree.nodes[nodeIdx].child == 0)
    return;
  
  CalculateNeighbours(nodeIdx, ipos);
  
  ivec3 size = ivec3(imageSize(u_diffuseColorImage));
  ivec3 parentBrickPos = BrickToImagePos(octree.nodes[nodeIdx].brick, size);
  for(int i = 0; i < 8; ++i)
  {
    uint childIdx = octree.nodes[nodeIdx].child + i;
    ivec3 childBrickPos = BrickToImagePos(octree.nodes[childIdx].brick, size);
    
    uint shift = MAX_DEPTH - u_layer - 2;
    ivec3 offset = (ivec3(i, i>>1, i>>2) & 1) << (shift + 1);
    
    DuplicateRight    (childIdx, ipos + offset, childBrickPos, shift);
    DuplicateTop      (childIdx, ipos + offset, childBrickPos, shift);
    DuplicateBack     (childIdx, ipos + offset, childBrickPos, shift);
    DuplicateTopBack  (childIdx, ipos + offset, childBrickPos, shift);
    DuplicateRightBack(childIdx, ipos + offset, childBrickPos, shift);
    DuplicateTopRight (childIdx, ipos + offset, childBrickPos, shift);
    DuplicateCorner   (childIdx, ipos + offset, childBrickPos, shift);
    
    int counter = 0;
    uvec4 colorAcc = uvec4(0);
    uvec4 normalAcc = uvec4(0);
    uvec4 lightAcc = uvec4(0);
    for(int j = 0; j < 8; ++j)
    {
      //calculate the position of the voxel in the 3D texture
      ivec3 childTexelPos = childBrickPos + (ivec3(j, j>>1, j>>2) & 1);
      
      //read the color value from the child
      uint readColor = imageLoad(u_diffuseColorImage, childTexelPos).r;
      uint readNormal = imageLoad(u_normalImage, childTexelPos).r;
      uint readLight = imageLoad(u_lightImage, childTexelPos).r;
      vec4 spreadColor = vec4((readColor >> 16) & 0xff,
                              (readColor >>  8) & 0xff,
                              (readColor >>  0) & 0xff,
                              (readColor >> 24) & 0xff) / 255.0f;
      vec4 spreadNormal = vec4((readNormal >> 16) & 0xff,
                               (readNormal >>  8) & 0xff,
                               (readNormal >>  0) & 0xff,
                               (readNormal >> 24) & 0xff) / 255.0f;
      vec4 spreadLight = vec4((readLight >> 16) & 0xff,
                              (readLight >>  8) & 0xff,
                              (readLight >>  0) & 0xff,
                              (readLight >> 24) & 0xff) / 255.0f;
      
      if(u_layer == MAX_DEPTH - 2 && spreadColor.a > 0)
      {
        imageStore(u_diffuseColorImage, childTexelPos, uvec4(readColor | 0xff000000, 0, 0, 0));
        imageStore(u_normalImage, childTexelPos, uvec4(readNormal | 0xff000000, 0, 0, 0));
        imageStore(u_lightImage, childTexelPos, uvec4(readLight | 0xff000000, 0, 0, 0)); //this should not be necessary
        spreadColor.a = 1.0f;
        spreadNormal.a = 1.0f;
        spreadLight.a = 1.0f; //also not necessary
      }
            
      //spreadColor.rgb *= spreadColor.a;
      //spreadNormal.rgb *= spreadColor.a;
      
      
      colorAcc += uvec4(spreadColor * 255.0f);
      normalAcc += uvec4(spreadNormal * 255.0f);
      
      //if(spreadColor.a > 0)
      {    
        //add color to the accumulator
        lightAcc += uvec4(spreadLight.rgb * 255.0f, spreadLight.a * 255.0f);
        
        //lightAcc = max(uvec4(spreadLight * 255.0f), lightAcc);
        counter++;
      }
    }
    

    
    //if there are no colors in the accumulator, we can skip writing to the parent texel
    if(counter == 0)
      continue;
    
    colorAcc.rgba /= 8;
    normalAcc.rgba /= 8;
    lightAcc.rgba /= counter;
    
    uint color = ((colorAcc.r & 0xff) << 16) | ((colorAcc.g & 0xff) << 8) | ((colorAcc.b & 0xff) << 0) | ((colorAcc.a & 0xff) << 24);
    uint normal = ((normalAcc.r & 0xff) << 16) | ((normalAcc.g & 0xff) << 8) | ((normalAcc.b & 0xff) << 0) | ((normalAcc.a & 0xff) << 24);
    uint light = ((lightAcc.r & 0xff) << 16) | ((lightAcc.g & 0xff) << 8) | ((lightAcc.b & 0xff) << 0) | ((lightAcc.a & 0xff) << 24);
    
    
    
    ivec3 parentTexelPos = parentBrickPos + (ivec3(i,i>>1,i>>2) & 1);
    imageStore(u_diffuseColorImage, parentTexelPos, uvec4(color, 0, 0, 0));
    imageStore(u_normalImage, parentTexelPos, uvec4(normal, 0, 0, 0));
    imageStore(u_lightImage, parentTexelPos, uvec4(light, 0, 0, 0));
  }
}
