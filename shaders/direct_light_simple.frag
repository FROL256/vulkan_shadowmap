#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;

} surf;

layout (binding = 0) uniform sampler2D  diffColor;
layout (binding = 1) uniform sampler2D  shadowMap;
layout (binding = 2) uniform usampler2D rotMap;

layout(push_constant) uniform params_t
{
  mat4 mWorldViewProj;
  mat4 mWorldLightProj;
  mat4 mNormalMatrix; 
    
  vec4 wCamPos;
  vec4 lightDir;

  vec2  texRotSize;
  float shadowMapSizeInv;
  float dummy1;

} params;

void main()
{
  const vec4 posLightClipSpace = params.mWorldLightProj*vec4(surf.wPos, 1.0f); // 
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]               

  const bool  outOfView = (shadowTexCoord.x < 0.001f || shadowTexCoord.x > 0.999f || shadowTexCoord.y < 0.001f || shadowTexCoord.y > 0.999f);
  const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;  

  const float dpFactor = max(dot(params.lightDir.xyz, surf.wNorm), 0.0f);
  const float cosPower = 120.0f;
  const vec3  v        = normalize(surf.wPos - params.wCamPos.xyz);
  const vec3  r        = reflect(v, surf.wNorm);
  const float cosAlpha = clamp(dot(params.lightDir.xyz, r), 0.0f, 1.0f);

  out_fragColor = texture(diffColor, surf.texCoord*2.0f)*(dpFactor*shadow + 0.25f) + vec4(1,1,1,1)*pow(cosAlpha, cosPower)*dpFactor*shadow;
}