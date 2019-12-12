#ifndef QMC_SOBOL_H
#define QMC_SOBOL_H

#include <vector>


#define QRNG_DIMENSIONS 4
#define QRNG_RESOLUTION 31
#define INT_SCALE (1.0f / (float)0x80000001U)

static inline float rndQmcSobolN(unsigned int pos, int dim, const unsigned int* c_Table)
{
  unsigned int result = 0;
  unsigned int data   = pos;

  for (int bit = 0; bit < QRNG_RESOLUTION; bit++, data >>= 1)
    if (data & 1) result ^= c_Table[bit + dim*QRNG_RESOLUTION];

  return (float)(result + 1) * INT_SCALE;
}


void initQuasirandomGenerator(unsigned int table[QRNG_DIMENSIONS][QRNG_RESOLUTION]);

std::vector<float>    MakeSortedByPixel_QRND_2D_DISK(int a_width, int a_height, int a_randsPerPixel);
std::vector<uint32_t> Compress8SamplesToUint32x4(const std::vector<float>& samplesF, int a_tex_width, int a_tex_height);

inline float rnd_simple(float s, float e) // not used actually! just for test
{
  float t = (float)(rand()) / (float)RAND_MAX;
  return s + t * (e - s);
}

#endif

