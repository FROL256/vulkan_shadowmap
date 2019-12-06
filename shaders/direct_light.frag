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

layout (binding = 0) uniform sampler2D diffColor;
layout (binding = 1) uniform sampler2D shadowMap;

layout(push_constant) uniform params_t
{
  mat4 mWorldViewProj;
  mat4 mWorldLightProj;  
  vec4 wCamPos;
  vec4 lightDir;
  vec4 lightPlaneEq;

} params;

void main()
{
  const float dpFactor      = max(dot(params.lightDir.xyz, surf.wNorm), 0.0f);
  const float lightDistMult = params.lightDir.w;
  const float distToLightPlane = dot(params.lightPlaneEq.xyz, surf.wPos) + params.lightPlaneEq.w / sqrt( dot(params.lightPlaneEq.xyz,params.lightPlaneEq.xyz) );

  const vec4 posLightClipSpace = params.mWorldLightProj*vec4(surf.wPos, 1.0f);
  //const vec2 shadowTexCoord    = (posLightClipSpace.xy/posLightClipSpace.w)*0.5f + vec2(0.5f, 0.5f);
  const vec2 shadowTexCoord    = (posLightClipSpace.xy)*0.5f + vec2(0.5f, 0.5f);    // ortographic projection

  const float shadowDepth      = textureLod(shadowMap, shadowTexCoord, 0).x*params.lightDir.w; // lightDir.w stores light box depth size
  const float bias             = 0.01f;
  const float shadow           = (distToLightPlane < shadowDepth + bias) ? 1.0f : 0.0f;
  //const float shadow = textureLod(shadowMap, shadowTexCoord, 0).x;

  const float cosPower = 120.0f;
  const vec3  v        = normalize(surf.wPos - params.wCamPos.xyz);
  const vec3  r        = reflect(v, surf.wNorm);
  const float cosAlpha = clamp(dot(params.lightDir.xyz, r), 0.0f, 1.0f);

  out_fragColor = texture(diffColor, surf.texCoord*2.0f)*(dpFactor*shadow + 0.05f) + vec4(1,1,1,1)*pow(cosAlpha, cosPower)*shadow;
}