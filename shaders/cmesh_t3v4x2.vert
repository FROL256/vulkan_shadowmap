#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
  mat4 mWorldViewProj;
  mat4 mWorldLightProj; 
  mat4 mNormalMatrix; 

  vec4 wCamPos;
  vec4 lightDir;
  vec4 lightPlaneEq;

} params;

vec3 DecodeNormal(uint a_data)
{  
  const uint a_enc_x = (a_data  & 0x0000FFFF);
  const uint a_enc_y = ((a_data & 0xFFFF0000) >> 16);
  const float sign   = (a_enc_x & 0x0001) != 0 ? -1.0f : 1.0f;
  
  const int usX = int(a_enc_x & 0x0000fffe);
  const int usY = int(a_enc_y & 0x0000ffff);

  const int sX  = (usX <= 32767) ? usX : usX - 65536;
  const int sY  = (usY <= 32767) ? usY : usY - 65536;

  const float x = sX*(1.0f / 32767.0f);
  const float y = sY*(1.0f / 32767.0f);
  const float z = sign*sqrt(max(1.0f - x*x - y*y, 0.0f));

  return vec3(x, y, z);
}

layout (location = 0 ) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;

} vOut;

void main(void)
{ 
  const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
  const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

  vOut.wPos     = vPosNorm.xyz;
  vOut.wNorm    = (params.mNormalMatrix*wNorm).xyz;
  vOut.wTangent = (params.mNormalMatrix*wTang).xyz;
  vOut.texCoord = vTexCoordAndTang.xy;

  gl_Position   = params.mWorldViewProj*vec4(vOut.wPos, 1.0);
}
