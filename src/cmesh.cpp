#include "cmesh.h"
#include "cmesh_vsgf.h"

#include <cmath>
#include <fstream>


std::vector<int> CreateQuadTriIndices(const int a_sizeX, const int a_sizeY)
{
  std::vector<int> indicesData(a_sizeY*a_sizeX * 6);
  int* indexBuf = indicesData.data();

  for (int i = 0; i < a_sizeY; i++)
  {
    for (int j = 0; j < a_sizeX; j++)
    {
      *indexBuf++ = (i + 0) * (a_sizeX + 1) + (j + 0);
      *indexBuf++ = (i + 1) * (a_sizeX + 1) + (j + 0);
      *indexBuf++ = (i + 1) * (a_sizeX + 1) + (j + 1);  
      *indexBuf++ = (i + 0) * (a_sizeX + 1) + (j + 0);
      *indexBuf++ = (i + 1) * (a_sizeX + 1) + (j + 1);
      *indexBuf++ = (i + 0) * (a_sizeX + 1) + (j + 1);
    }
  }

  return indicesData;
}

cmesh::SimpleMesh cmesh::CreateQuad(const int a_sizeX, const int a_sizeY, const float a_size)
{
  const int vertNumX = a_sizeX + 1;
  const int vertNumY = a_sizeY + 1;

  const int quadsNum = a_sizeX*a_sizeY;
  const int vertNum  = vertNumX*vertNumY;

  cmesh::SimpleMesh res(vertNum, quadsNum*2*3);

  const float edgeLength  = a_size / float(a_sizeX);
  const float edgeLength2 = sqrtf(2.0f)*edgeLength;

  // put vertices
  //
  const float startX = -0.5f*a_size;
  const float startY = -0.5f*a_size;

  for (int y = 0; y < vertNumY; y++)
  {
    const float ypos = startY + float(y)*edgeLength;
    const int offset = y*vertNumX;

    for (int x = 0; x < vertNumX; x++)
    { 
      const float xpos = startX + float(x)*edgeLength;
      const int i      = offset + x;

      res.vPos4f [i * 4 + 0] = xpos;
      res.vPos4f [i * 4 + 1] = ypos;
      res.vPos4f [i * 4 + 2] = 0.0f;
      res.vPos4f [i * 4 + 3] = 1.0f;

      res.vNorm4f[i * 4 + 0] = 0.0f;
      res.vNorm4f[i * 4 + 1] = 0.0f;
      res.vNorm4f[i * 4 + 2] = 1.0f;
      res.vNorm4f[i * 4 + 3] = 0.0f;

      res.vTexCoord2f[i*2 + 0] = (xpos - startX) / a_size;
      res.vTexCoord2f[i*2 + 1] = (ypos - startY) / a_size;
    }
  }
 
  res.indices = CreateQuadTriIndices(a_sizeX, a_sizeY);

  return res;
}

cmesh::SimpleMesh cmesh::LoadMeshFromVSGF(const char* a_fileName)
{
  std::ifstream input(a_fileName, std::ios::binary);
  if(!input.is_open())
    return SimpleMesh();

  cmesh::HydraGeomData data;
  data.read(input); 

  SimpleMesh res(data.getVerticesNumber(), data.getIndicesNumber());

  memcpy(res.vPos4f.data(),      data.getVertexPositionsFloat4Array(), res.vPos4f.size()*sizeof(float));
  memcpy(res.vNorm4f.data(),     data.getVertexNormalsFloat4Array(),   res.vNorm4f.size()*sizeof(float));
  
  if(data.getVertexTangentsFloat4Array() != nullptr)
    memcpy(res.vTang4f.data(),     data.getVertexTangentsFloat4Array(),  res.vTang4f.size()*sizeof(float));
  else
    memset(res.vTang4f.data(), 0, res.vTang4f.size()*sizeof(float));

  memcpy(res.vTexCoord2f.data(), data.getVertexTexcoordFloat2Array(),  res.vTexCoord2f.size()*sizeof(float));

  memcpy(res.indices.data(),     data.getTriangleVertexIndicesArray(),   res.indices.size()*sizeof(int));
  memcpy(res.matIndices.data(),  data.getTriangleMaterialIndicesArray(), res.matIndices.size()*sizeof(int));

  return res; 
}