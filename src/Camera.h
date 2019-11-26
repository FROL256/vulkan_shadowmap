// © Copyright 2017 Vladimir Frolov, Ray Tracing Systems
//
#pragma once

#include "LiteMath.h"

struct Camera
{
  Camera() : pos(0.0f, 1.0f, +5.0f), lookAt(0, 0, 0), up(0, 1, 0), fov(45.0f), tdist(100.0f) {}

  LiteMath::float3 pos;
  LiteMath::float3 lookAt;
  LiteMath::float3 up;
  float  fov;
  float  tdist;

  LiteMath::float3 forward() const { return LiteMath::normalize(lookAt - pos); }
  LiteMath::float3 right()   const { return LiteMath::cross(forward(), up); }

  void offsetOrientation(float a_upAngle, float rightAngle)
  {
    if (a_upAngle != 0.0f)  // rotate vertical
    {
      LiteMath::float3 direction = LiteMath::normalize(forward() * cosf(-TORADIANS*a_upAngle) + up * sinf(-TORADIANS*a_upAngle));

      up     = LiteMath::normalize(LiteMath::cross(right(), direction));
      lookAt = pos + tdist*direction;
    }

    if (rightAngle != 0.0f)  // rotate horizontal
    {
      LiteMath::float4x4 rot;
      rot.identity();
      rot.M(0, 0) = rot.M(2, 2) = cosf(-TORADIANS*rightAngle);
      rot.M(0, 2) = -sinf(-TORADIANS*rightAngle);
      rot.M(2, 0) = sinf(-TORADIANS*rightAngle);

      LiteMath::float3 direction2 = LiteMath::normalize(LiteMath::mul(rot, forward()));
      up     = LiteMath::normalize(LiteMath::mul(rot, up));
      lookAt = pos + tdist*direction2;
    }
  }

  void offsetPosition(LiteMath::float3 a_offset)
  {
    pos    += a_offset;
    lookAt += a_offset;
  }

protected:
  const float TORADIANS = 0.01745329251994329576923690768489f;
};