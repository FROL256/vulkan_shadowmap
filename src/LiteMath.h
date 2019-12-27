#ifndef VFLOAT4_ALL_H
#define VFLOAT4_ALL_H

// This is just and example. 
// In practise you may take any of these files that you prefer for your platform.  
// Or customise this file as you like.

#ifdef WIN32
  #include "vfloat4_x64.h"
#else
  #ifdef __arm__
    #include "vfloat4_arm.h"
  #else
    #include "vfloat4_gcc.h"
  #endif  
#endif 

// __mips__
// __ppc__ 

namespace cmath
{ 
  const float EPSILON    = 1e-6f;
  const float DEG_TO_RAD = float(3.14159265358979323846f) / 180.0f;
  
  typedef cvex::vint4     int4;  // #TODO: create convenient interface if needed
  typedef cvex::vuint4    uint4; // #TODO: create convenient interface if needed

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  struct float4
  {
    inline float4() : x(0), y(0), z(0), w(0) {}
    inline float4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
    inline explicit float4(float a[4]) : x(a[0]), y(a[1]), z(a[2]), w(a[3]) {}

    inline float4(cvex::vfloat4 rhs) { v = rhs; }
    inline float4 operator=(cvex::vfloat4 rhs) { v = rhs; return *this; }
    inline operator cvex::vfloat4() const { return v; }
    
    inline float& operator[](int i)       { return v[i]; }
    inline float  operator[](int i) const { return v[i]; }

    inline float4 operator+(const float4& b) { return v + b.v; }
    inline float4 operator-(const float4& b) { return v - b.v; }
    inline float4 operator*(const float4& b) { return v * b.v; }
    inline float4 operator/(const float4& b) { return v / b.v; }

    inline float4 operator+(const float rhs) { return v + rhs; }
    inline float4 operator-(const float rhs) { return v - rhs; }
    inline float4 operator*(const float rhs) { return v * rhs; }
    inline float4 operator/(const float rhs) { return v / rhs; }

    inline cvex::vint4 operator> (const float4& b) { return (v > b.v); }
    inline cvex::vint4 operator< (const float4& b) { return (v < b.v); }
    inline cvex::vint4 operator>=(const float4& b) { return (v >= b.v); }
    inline cvex::vint4 operator<=(const float4& b) { return (v <= b.v); }
    inline cvex::vint4 operator==(const float4& b) { return (v == b.v); }
    inline cvex::vint4 operator!=(const float4& b) { return (v != b.v); }

    union
    {
      struct {float x, y, z, w; };
      cvex::vfloat4 v;
    };
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  struct float2
  {
    inline float2() :x(0), y(0) {}
    inline float2(float a, float b) : x(a), y(b) {}
    inline explicit float2(float a[2]) : x(a[0]), y(a[1]) {}

    inline float& operator[](int i)       { return M[i]; }
    inline float  operator[](int i) const { return M[i]; }

    union
    {
      struct {float x, y; };
      float M[2];
    };
  };

  struct float3
  {
    inline float3() :x(0), y(0), z(0) {}
    inline float3(float a, float b, float c) : x(a), y(b), z(c) {}
    inline explicit float3(const float* ptr) : x(ptr[0]), y(ptr[1]), z(ptr[2]) {}

    inline float& operator[](int i)       { return M[i]; }
    inline float  operator[](int i) const { return M[i]; }

    union
    {
      struct {float x, y, z; };
      float M[3];
    };
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /**
  \brief this class use colmajor memory layout for effitient vector-matrix operations
  */
  struct float4x4
  {
    inline float4x4() 
    {
      m_col[0] = float4{ 1.0f, 0.0f, 0.0f, 0.0f };
      m_col[1] = float4{ 0.0f, 1.0f, 0.0f, 0.0f };
      m_col[2] = float4{ 0.0f, 0.0f, 1.0f, 0.0f };
      m_col[3] = float4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    inline float4x4(const float A[16])
    {
      m_col[0] = float4{ A[0], A[4], A[8], A[12] };
      m_col[1] = float4{ A[1], A[5], A[9], A[13] };
      m_col[2] = float4{ A[2], A[6], A[10], A[14] };
      m_col[3] = float4{ A[3], A[7], A[11], A[15] };
    }

    inline float4x4 operator*(const float4x4& rhs)
    {
      // transpose will change multiplication order (due to in fact we use column major)
      //
      float4x4 res;
      cvex::mat4_rowmajor_mul_mat4((float*)res.m_col, (const float*)rhs.m_col, (const float*)m_col); 
      return res;
    }

    inline float4 get_col(int i) const         { return m_col[i]; }
    inline void   set_col(int i, float4 a_col) { m_col[i] = a_col; }

    inline float4 get_row(int i) const { return float4{ m_col[0][i], m_col[1][i], m_col[2][i], m_col[3][i] }; }
    inline void   set_row(int i, float4 a_col)
    {
      m_col[0][i] = a_col[0];
      m_col[1][i] = a_col[1];
      m_col[2][i] = a_col[2];
      m_col[3][i] = a_col[3];
    }

    inline float4& col(int i)       { return m_col[i]; }
    inline float4  col(int i) const { return m_col[i]; }

    inline float& operator()(int row, int col)       { return m_col[col][row]; }
    inline float  operator()(int row, int col) const { return m_col[col][row]; }

  private:
    float4 m_col[4];
  };

  static inline float4 operator*(const float4x4& m, const float4& v)
  {
    float4 res;
    cvex::mat4_colmajor_mul_vec4((float*)&res, (const float*)&m, (const float*)&v);
    return res;
  }

  static inline float4x4 transpose(const float4x4& rhs)
  {
    float4x4 res;
    cvex::transpose4((const cvex::vfloat4*)&rhs, (cvex::vfloat4*)&res);
    return res;
  }

  static inline float4x4 translate4x4(float3 t)
  {
    float4x4 res;
    res.set_col(3, float4{t.x,  t.y,  t.z, 1.0f });
    return res;
  }

  static inline float4x4 scale4x4(float3 t)
  {
    float4x4 res;
    res.set_col(0, float4{t.x, 0.0f, 0.0f,  0.0f});
    res.set_col(1, float4{0.0f, t.y, 0.0f,  0.0f});
    res.set_col(2, float4{0.0f, 0.0f,  t.z, 0.0f});
    res.set_col(3, float4{0.0f, 0.0f, 0.0f, 1.0f});
    return res;
  }

  static inline float4x4 rotate4x4X(float phi)
  {
    float4x4 res;
    res.set_col(0, float4{1.0f,      0.0f,       0.0f, 0.0f  });
    res.set_col(1, float4{0.0f, +cosf(phi),  +sinf(phi), 0.0f});
    res.set_col(2, float4{0.0f, -sinf(phi),  +cosf(phi), 0.0f});
    res.set_col(3, float4{0.0f,      0.0f,       0.0f, 1.0f  });
    return res;
  }

  static inline float4x4 rotate4x4Y(float phi)
  {
    float4x4 res;
    res.set_col(0, float4{+cosf(phi), 0.0f, -sinf(phi), 0.0f});
    res.set_col(1, float4{     0.0f, 1.0f,      0.0f, 0.0f  });
    res.set_col(2, float4{+sinf(phi), 0.0f, +cosf(phi), 0.0f});
    res.set_col(3, float4{     0.0f, 0.0f,      0.0f, 1.0f  });
    return res;
  }

  static inline float4x4 rotate4x4Z(float phi)
  {
    float4x4 res;
    res.set_col(0, float4{+cosf(phi), sinf(phi), 0.0f, 0.0f});
    res.set_col(1, float4{-sinf(phi), cosf(phi), 0.0f, 0.0f});
    res.set_col(2, float4{     0.0f,     0.0f, 1.0f, 0.0f  });
    res.set_col(3, float4{     0.0f,     0.0f, 0.0f, 1.0f  });
    return res;
  }
  
  static inline float4x4 inverse4x4(float4x4 m1)
  {
    CVEX_ALIGNED(16) float tmp[12]; // temp array for pairs
    float4x4 m;

    // calculate pairs for first 8 elements (cofactors)
    //
    tmp[0]  = m1(2,2) * m1(3,3);
    tmp[1]  = m1(2,3) * m1(3,2);
    tmp[2]  = m1(2,1) * m1(3,3);
    tmp[3]  = m1(2,3) * m1(3,1);
    tmp[4]  = m1(2,1) * m1(3,2);
    tmp[5]  = m1(2,2) * m1(3,1);
    tmp[6]  = m1(2,0) * m1(3,3);
    tmp[7]  = m1(2,3) * m1(3,0);
    tmp[8]  = m1(2,0) * m1(3,2);
    tmp[9]  = m1(2,2) * m1(3,0);
    tmp[10] = m1(2,0) * m1(3,1);
    tmp[11] = m1(2,1) * m1(3,0);

    // calculate first 8 m1.rowents (cofactors)
    //
    m(0,0) = tmp[0]  * m1(1,1) + tmp[3] * m1(1,2) + tmp[4]  * m1(1,3);
    m(0,0) -= tmp[1] * m1(1,1) + tmp[2] * m1(1,2) + tmp[5]  * m1(1,3);
    m(1,0) = tmp[1]  * m1(1,0) + tmp[6] * m1(1,2) + tmp[9]  * m1(1,3);
    m(1,0) -= tmp[0] * m1(1,0) + tmp[7] * m1(1,2) + tmp[8]  * m1(1,3);
    m(2,0) = tmp[2]  * m1(1,0) + tmp[7] * m1(1,1) + tmp[10] * m1(1,3);
    m(2,0) -= tmp[3] * m1(1,0) + tmp[6] * m1(1,1) + tmp[11] * m1(1,3);
    m(3,0) = tmp[5]  * m1(1,0) + tmp[8] * m1(1,1) + tmp[11] * m1(1,2);
    m(3,0) -= tmp[4] * m1(1,0) + tmp[9] * m1(1,1) + tmp[10] * m1(1,2);
    m(0,1) = tmp[1]  * m1(0,1) + tmp[2] * m1(0,2) + tmp[5]  * m1(0,3);
    m(0,1) -= tmp[0] * m1(0,1) + tmp[3] * m1(0,2) + tmp[4]  * m1(0,3);
    m(1,1) = tmp[0]  * m1(0,0) + tmp[7] * m1(0,2) + tmp[8]  * m1(0,3);
    m(1,1) -= tmp[1] * m1(0,0) + tmp[6] * m1(0,2) + tmp[9]  * m1(0,3);
    m(2,1) = tmp[3]  * m1(0,0) + tmp[6] * m1(0,1) + tmp[11] * m1(0,3);
    m(2,1) -= tmp[2] * m1(0,0) + tmp[7] * m1(0,1) + tmp[10] * m1(0,3);
    m(3,1) = tmp[4]  * m1(0,0) + tmp[9] * m1(0,1) + tmp[10] * m1(0,2);
    m(3,1) -= tmp[5] * m1(0,0) + tmp[8] * m1(0,1) + tmp[11] * m1(0,2);

    // calculate pairs for second 8 m1.rowents (cofactors)
    //
    tmp[0]  = m1(0,2) * m1(1,3);
    tmp[1]  = m1(0,3) * m1(1,2);
    tmp[2]  = m1(0,1) * m1(1,3);
    tmp[3]  = m1(0,3) * m1(1,1);
    tmp[4]  = m1(0,1) * m1(1,2);
    tmp[5]  = m1(0,2) * m1(1,1);
    tmp[6]  = m1(0,0) * m1(1,3);
    tmp[7]  = m1(0,3) * m1(1,0);
    tmp[8]  = m1(0,0) * m1(1,2);
    tmp[9]  = m1(0,2) * m1(1,0);
    tmp[10] = m1(0,0) * m1(1,1);
    tmp[11] = m1(0,1) * m1(1,0);

    // calculate second 8 m1 (cofactors)
    //
    m(0,2) = tmp[0]   * m1(3,1) + tmp[3]  * m1(3,2) + tmp[4]  * m1(3,3);
    m(0,2) -= tmp[1]  * m1(3,1) + tmp[2]  * m1(3,2) + tmp[5]  * m1(3,3);
    m(1,2) = tmp[1]   * m1(3,0) + tmp[6]  * m1(3,2) + tmp[9]  * m1(3,3);
    m(1,2) -= tmp[0]  * m1(3,0) + tmp[7]  * m1(3,2) + tmp[8]  * m1(3,3);
    m(2,2) = tmp[2]   * m1(3,0) + tmp[7]  * m1(3,1) + tmp[10] * m1(3,3);
    m(2,2) -= tmp[3]  * m1(3,0) + tmp[6]  * m1(3,1) + tmp[11] * m1(3,3);
    m(3,2) = tmp[5]   * m1(3,0) + tmp[8]  * m1(3,1) + tmp[11] * m1(3,2);
    m(3,2) -= tmp[4]  * m1(3,0) + tmp[9]  * m1(3,1) + tmp[10] * m1(3,2);
    m(0,3) = tmp[2]   * m1(2,2) + tmp[5]  * m1(2,3) + tmp[1]  * m1(2,1);
    m(0,3) -= tmp[4]  * m1(2,3) + tmp[0]  * m1(2,1) + tmp[3]  * m1(2,2);
    m(1,3) = tmp[8]   * m1(2,3) + tmp[0]  * m1(2,0) + tmp[7]  * m1(2,2);
    m(1,3) -= tmp[6]  * m1(2,2) + tmp[9]  * m1(2,3) + tmp[1]  * m1(2,0);
    m(2,3) = tmp[6]   * m1(2,1) + tmp[11] * m1(2,3) + tmp[3]  * m1(2,0);
    m(2,3) -= tmp[10] * m1(2,3) + tmp[2]  * m1(2,0) + tmp[7]  * m1(2,1);
    m(3,3) = tmp[10]  * m1(2,2) + tmp[4]  * m1(2,0) + tmp[9]  * m1(2,1);
    m(3,3) -= tmp[8]  * m1(2,1) + tmp[11] * m1(2,2) + tmp[5]  * m1(2,0);

    // calculate matrix inverse
    //
    const float k = 1.0f / (m1(0,0) * m(0,0) + m1(0,1) * m(1,0) + m1(0,2) * m(2,0) + m1(0,3) * m(3,0));
    const float4 vK{k,k,k,k};

    m.set_col(0, m.get_col(0)*vK);
    m.set_col(1, m.get_col(1)*vK);
    m.set_col(2, m.get_col(2)*vK);
    m.set_col(3, m.get_col(3)*vK);

    return m;
  }
  
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  static inline float clamp(float u, float a, float b) { const float r = fmax(a, u);      return fmin(r, b); }

  static inline int imax  (int a, int b)        { return a > b ? a : b; }                                    // OpenCL C does not allow overloading!
  static inline int imin  (int a, int b)        { return a < b ? a : b; }                                    // OpenCL C does not allow overloading!
  static inline int iclamp(int u, int a, int b) { const int   r = (a > u) ? a : u; return (r < b) ? r : b; } // OpenCL C does not allow overloading!

  inline float rnd(float s, float e)
  {
    const float t = (float)(rand()) / (float)RAND_MAX;
    return s + t*(e - s);
  }

  template<typename T> inline T SQR(T x) { return x * x; }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  static inline float2 to_float2(float4 v)          { return float2{v.x, v.y}; }
  static inline float2 to_float2(float3 v)          { return float2{v.x, v.y}; }
  static inline float3 to_float3(float4 v)          { return float3{v.x, v.y, v.z}; }
  static inline float4 to_float4(float3 v, float w) { return float4{v.x, v.y, v.z, w}; }

  //**********************************************************************************
  // float3 operators and functions
  //**********************************************************************************
  static inline float3 operator * (const float3 & u, float v) { return float3{u.x * v, u.y * v, u.z * v}; }
  static inline float3 operator / (const float3 & u, float v) { return float3{u.x / v, u.y / v, u.z / v}; }
  static inline float3 operator + (const float3 & u, float v) { return float3{u.x + v, u.y + v, u.z + v}; }
  static inline float3 operator - (const float3 & u, float v) { return float3{u.x - v, u.y - v, u.z - v}; }
  static inline float3 operator * (float v, const float3 & u) { return float3{v * u.x, v * u.y, v * u.z}; }
  static inline float3 operator / (float v, const float3 & u) { return float3{v / u.x, v / u.y, v / u.z}; }
  static inline float3 operator + (float v, const float3 & u) { return float3{u.x + v, u.y + v, u.z + v}; }
  static inline float3 operator - (float v, const float3 & u) { return float3{u.x - v, u.y - v, u.z - v}; }

  static inline float3 operator + (const float3 & u, const float3 & v) { return float3{u.x + v.x, u.y + v.y, u.z + v.z}; }
  static inline float3 operator - (const float3 & u, const float3 & v) { return float3{u.x - v.x, u.y - v.y, u.z - v.z}; }
  static inline float3 operator * (const float3 & u, const float3 & v) { return float3{u.x * v.x, u.y * v.y, u.z * v.z}; }
  static inline float3 operator / (const float3 & u, const float3 & v) { return float3{u.x / v.x, u.y / v.y, u.z / v.z}; }

  static inline float3 operator - (const float3 & u) { return {-u.x, -u.y, -u.z}; }

  static inline float3 & operator += (float3 & u, const float3 & v) { u.x += v.x; u.y += v.y; u.z += v.z; return u; }
  static inline float3 & operator -= (float3 & u, const float3 & v) { u.x -= v.x; u.y -= v.y; u.z -= v.z; return u; }
  static inline float3 & operator *= (float3 & u, const float3 & v) { u.x *= v.x; u.y *= v.y; u.z *= v.z; return u; }
  static inline float3 & operator /= (float3 & u, const float3 & v) { u.x /= v.x; u.y /= v.y; u.z /= v.z; return u; }

  static inline float3 & operator += (float3 & u, float v) { u.x += v; u.y += v; u.z += v; return u; }
  static inline float3 & operator -= (float3 & u, float v) { u.x -= v; u.y -= v; u.z -= v; return u; }
  static inline float3 & operator *= (float3 & u, float v) { u.x *= v; u.y *= v; u.z *= v; return u; }
  static inline float3 & operator /= (float3 & u, float v) { u.x /= v; u.y /= v; u.z /= v; return u; }
  static inline bool     operator == (const float3 & u, const float3 & v) { return (fabs(u.x - v.x) < EPSILON) && (fabs(u.y - v.y) < EPSILON) &&
                                                                                   (fabs(u.z - v.z) < EPSILON); }
  static inline float3 lerp(const float3 & u, const float3 & v, float t) { return u + t * (v - u); }
  static inline float  dot(const float3 & u, const float3 & v) { return (u.x*v.x + u.y*v.y + u.z*v.z); }
  static inline float3 cross(const float3 & u, const float3 & v) { return float3{u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x}; }
  static inline float3 clamp(const float3 & u, float a, float b) { return float3{clamp(u.x, a, b), clamp(u.y, a, b), clamp(u.z, a, b)}; }

  static inline float  length(const float3 & u) { return sqrtf(SQR(u.x) + SQR(u.y) + SQR(u.z)); }
  static inline float  lengthSquare(const float3 u) { return u.x*u.x + u.y*u.y + u.z*u.z; }
  static inline float3 normalize(const float3 & u) { return u / length(u); }

  static inline float  maxcomp(const float3 & u) { return fmax(u.x, fmax(u.y, u.z)); }
  static inline float  mincomp(const float3 & u) { return fmin(u.x, fmin(u.y, u.z)); }

  //**********************************************************************************
  // float2 operators and functions
  //**********************************************************************************

  static inline float2 operator * (const float2 & u, float v) { return float2{u.x * v, u.y * v}; }
  static inline float2 operator / (const float2 & u, float v) { return float2{u.x / v, u.y / v}; }
  static inline float2 operator * (float v, const float2 & u) { return float2{v * u.x, v * u.y}; }
  static inline float2 operator / (float v, const float2 & u) { return float2{v / u.x, v / u.y}; }

  static inline float2 operator + (const float2 & u, const float2 & v) { return float2{u.x + v.x, u.y + v.y}; }
  static inline float2 operator - (const float2 & u, const float2 & v) { return float2{u.x - v.x, u.y - v.y}; }
  static inline float2 operator * (const float2 & u, const float2 & v) { return float2{u.x * v.x, u.y * v.y}; }
  static inline float2 operator / (const float2 & u, const float2 & v) { return float2{u.x / v.x, u.y / v.y}; }
  static inline float2 operator - (const float2 & v) { return {-v.x, -v.y}; }

  static inline float2 & operator += (float2 & u, const float2 & v) { u.x += v.x; u.y += v.y; return u; }
  static inline float2 & operator -= (float2 & u, const float2 & v) { u.x -= v.x; u.y -= v.y; return u; }
  static inline float2 & operator *= (float2 & u, const float2 & v) { u.x *= v.x; u.y *= v.y; return u; }
  static inline float2 & operator /= (float2 & u, const float2 & v) { u.x /= v.x; u.y /= v.y; return u; }

  static inline float2 & operator += (float2 & u, float v) { u.x += v; u.y += v; return u; }
  static inline float2 & operator -= (float2 & u, float v) { u.x -= v; u.y -= v; return u; }
  static inline float2 & operator *= (float2 & u, float v) { u.x *= v; u.y *= v; return u; }
  static inline float2 & operator /= (float2 & u, float v) { u.x /= v; u.y /= v; return u; }
  static inline bool     operator == (const float2 & u, const float2 & v) { return (fabs(u.x - v.x) < EPSILON) && (fabs(u.y - v.y) < EPSILON); }

  static inline float2 lerp(const float2 & u, const float2 & v, float t) { return u + t * (v - u); }
  static inline float  dot(const float2 & u, const float2 & v)   { return (u.x*v.x + u.y*v.y); }
  static inline float2 clamp(const float2 & u, float a, float b) { return float2{clamp(u.x, a, b), clamp(u.y, a, b)}; }

  static inline float  length(const float2 & u)    { return sqrtf(SQR(u.x) + SQR(u.y)); }
  static inline float2 normalize(const float2 & u) { return u / length(u); }

  static inline float  lerp(float u, float v, float t) { return u + t * (v - u); }

  static inline float3 operator*(const float4x4& m, const float3& v)
  {
    float4 v2 = to_float4(v, 1.0f);
    float4 res;
    cvex::mat4_colmajor_mul_vec4((float*)&res, (const float*)&m, (const float*)&v2);
    return to_float3(res);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  struct int3
  {
      int3() :x(0), y(0), z(0) {}
      int3(int a, int b, int c) : x(a), y(b), z(c) {}

      int x, y, z;
  };

  struct uint3
  {
      uint3() :x(0), y(0), z(0) {}
      uint3(unsigned int a, unsigned int b, unsigned int c) : x(a), y(b), z(c) {}

      unsigned int x, y, z;
  };

  struct int2
  {
    int2() : x(0), y(0) {}
    int2(int a, int b) : x(a), y(b) {}

    int x, y;
  };

  struct uint2
  {
    uint2() : x(0), y(0) {}
    uint2(unsigned int a, unsigned int b) : x(a), y(b) {}

    bool operator==(const uint2 &other) const { return (x == other.x && y == other.y) || (x == other.y && y == other.x); }

    unsigned int x, y;
  };

  struct ushort2
  {
    ushort2() : x(0), y(0) {}
    ushort2(unsigned short a, unsigned short b) : x(a), y(b) {}

    unsigned short x, y;
  };

  struct ushort4
  {
    ushort4() :x(0), y(0), z(0), w(0) {}
    ushort4(unsigned short a, unsigned short b, unsigned short c, unsigned short d) : x(a), y(b), z(c), w(d) {}

    unsigned short x, y, z, w;
  };

  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  static inline bool IntersectBox2Box2(float2 box1Min, float2 box1Max, float2 box2Min, float2 box2Max)
  {
    return box1Min.x <= box2Max.x && box2Min.x <= box1Max.x &&
           box1Min.y <= box2Max.y && box2Min.y <= box1Max.y;
  }
 
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // Look At matrix creation
  // return the inverse view matrix
  //
  static inline float4x4 lookAt(float3 eye, float3 center, float3 up)
  {
    float3 x, y, z; // basis; will make a rotation matrix

    z.x = eye.x - center.x;
    z.y = eye.y - center.y;
    z.z = eye.z - center.z;
    z = normalize(z);

    y.x = up.x;
    y.y = up.y;
    y.z = up.z;

    x = cross(y, z); // X vector = Y cross Z
    y = cross(z, x); // Recompute Y = Z cross X

    // cross product gives area of parallelogram, which is < 1.0 for
    // non-perpendicular unit-length vectors; so normalize x, y here
    x = normalize(x);
    y = normalize(y);

    float4x4 M;
    M(0, 0) = x.x; M(1, 0) = x.y; M(2, 0) = x.z; M(3, 0) = -x.x * eye.x - x.y * eye.y - x.z*eye.z;
    M(0, 1) = y.x; M(1, 1) = y.y; M(2, 1) = y.z; M(3, 1) = -y.x * eye.x - y.y * eye.y - y.z*eye.z;
    M(0, 2) = z.x; M(1, 2) = z.y; M(2, 2) = z.z; M(3, 2) = -z.x * eye.x - z.y * eye.y - z.z*eye.z;
    M(0, 3) = 0.0; M(1, 3) = 0.0; M(2, 3) = 0.0; M(3, 3) = 1.0;
    return transpose(M);
  }

  static inline float4x4 projectionMatrix(float fovy, float aspect, float zNear, float zFar)
  {
    float4x4 res;
    const float ymax = zNear * tanf(fovy * 3.14159265358979323846f / 360.0f);
    const float xmax = ymax * aspect;

    const float left = -xmax;
    const float right = +xmax;
    const float bottom = -ymax;
    const float top = +ymax;

    const float temp = 2.0f * zNear;
    const float temp2 = right - left;
    const float temp3 = top - bottom;
    const float temp4 = zFar - zNear;

    res(0, 0) = temp / temp2;
    res(0, 1) = 0.0;
    res(0, 2) = 0.0;
    res(0, 3) = 0.0;

    res(1, 0) = 0.0;
    res(1, 1) = temp / temp3;
    res(1, 2) = 0.0;
    res(1, 3) = 0.0;

    res(2, 0) = (right + left) / temp2;
    res(2, 1) = (top + bottom) / temp3;
    res(2, 2) = (-zFar - zNear) / temp4;
    res(2, 3) = -1.0;

    res(3, 0) = 0.0;
    res(3, 1) = 0.0;
    res(3, 2) = (-temp * zFar) / temp4;
    res(3, 3) = 0.0;

    return transpose(res);
  }

  static inline float4x4 ortoMatrix(const float l, const float r, const float b, const float t, const float n, const float f)
  {
    float4x4 res;
    res(0,0) = 2.0f / (r - l);
    res(0,1) = 0;
    res(0,2) = 0;
    res(0,3) = -(r + l) / (r - l);

    res(1,0) = 0;
    res(1,1) = -2.0f / (t - b);  // why minus ??? check it for OpenGL please
    res(1,2) = 0;
    res(1,3) = -(t + b) / (t - b);

    res(2,0) = 0;
    res(2,1) = 0;
    res(2,2) = -2.0f / (f - n);
    res(2,3) = -(f + n) / (f - n);

    res(3,0) = 0.0f;
    res(3,1) = 0.0f;
    res(3,2) = 0.0f;
    res(3,3) = 1.0f;
    return res;
  }

  // http://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
  //
  static inline float4x4 OpenglToVulkanProjectionMatrixFix()
  {
    float4x4 res;
    res(1,1) = -1.0f;
    res(2,2) = 0.5f;
    res(2,3) = 0.5f;
    return transpose(res);
  }

};

#endif
