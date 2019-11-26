//////////////////////////////////
// Created by frol on 23.11.19. //
//////////////////////////////////

#ifndef CMESH_GEOM_H
#define CMESH_GEOM_H

#include <vector>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <cassert>

namespace cmesh
{
  // very simple utility mesh representation for working with geometry on the CPU in C++
  //
  struct SimpleMesh
  {
    SimpleMesh(){}
    SimpleMesh(int a_vertNum, int a_indNum) { Resize(a_vertNum, a_indNum); }

    inline size_t VerticesNum()  const { return vPos4f.size()/4; }
    inline size_t IndicesNum()   const { return indices.size();  }
    inline size_t TrianglesNum() const { return IndicesNum()/3;  }
    inline void   Resize(int a_vertNum, int a_indNum) 
    {
      vPos4f.resize(a_vertNum*4);
      vNorm4f.resize(a_vertNum*4);
      vTang4f.resize(a_vertNum*4);
      vTexCoord2f.resize(a_vertNum*2);
      indices.resize(a_indNum);
      matIndices.resize(a_indNum/3); 
      assert(a_indNum%3 == 0); // PLEASE NOTE THAT CURRENT IMPLEMENTATION ASSUME ONLY TRIANGLE MESHES! 
    };

    std::vector<float> vPos4f;      // 
    std::vector<float> vNorm4f;     // 
    std::vector<float> vTang4f;     // 
    std::vector<float> vTexCoord2f; // 
    std::vector<int>   indices;     // size = 3*TrianglesNum() for triangle mesh, 4*TrianglesNum() for quad mesh
    std::vector<int>   matIndices;  // size = 1*TrianglesNum()
    
    //enum SIMPLE_MESH_TOPOLOGY {SIMPLE_MESH_TRIANGLES = 0, SIMPLE_MESH_QUADS = 1};
    //SIMPLE_MESH_TOPOLOGY topology = SIMPLE_MESH_TRIANGLES;
  };

  SimpleMesh LoadMeshFromVSGF(const char* a_fileName);
  SimpleMesh CreateQuad(const int a_sizeX, const int a_sizeY, const float a_size);

  //struct MultiIndexMesh
  //{
  //  struct Triangle
  //  {
  //    int posInd[3];      // positions
  //    int normInd[3];     // both for normals and tangents ?
  //    int texCoordInd[3]; // tex coords
  //  };
  //
  //  std::vector<float> vPos4f;      // 
  //  std::vector<float> vNorm4f;     // 
  //  std::vector<float> vTang4f;     // 
  //  std::vector<float> vTexCoord2f; //    
  //};

};


#endif // CMESH_GEOM_H
