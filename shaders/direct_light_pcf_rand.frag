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
  
  vec2  isTexCoord  = mod(gl_FragCoord.xy, params.texRotSize);
  const uvec4 data  = texelFetch(rotMap, ivec2(isTexCoord.x,isTexCoord.y) , 0);
  const float nMult = (1.0f/255.0f);

  const float s0x = float((data.x & 0x000000FF)     )*nMult*2.0f - 1.0f;
  const float s0y = float((data.x & 0x0000FF00) >> 8)*nMult*2.0f - 1.0f;
  const float s1x = float((data.x & 0x00FF0000) >> 16)*nMult*2.0f - 1.0f;
  const float s1y = float((data.x & 0xFF000000) >> 24)*nMult*2.0f - 1.0f;

  const float s2x = float((data.y & 0x000000FF)     )*nMult*2.0f - 1.0f;
  const float s2y = float((data.y & 0x0000FF00) >> 8)*nMult*2.0f - 1.0f;
  const float s3x = float((data.y & 0x00FF0000) >> 16)*nMult*2.0f - 1.0f;
  const float s3y = float((data.y & 0xFF000000) >> 24)*nMult*2.0f - 1.0f;

  const float s4x = float((data.z & 0x000000FF)     )*nMult*2.0f - 1.0f;
  const float s4y = float((data.z & 0x0000FF00) >> 8)*nMult*2.0f - 1.0f;
  const float s5x = float((data.z & 0x00FF0000) >> 16)*nMult*2.0f - 1.0f;
  const float s5y = float((data.z & 0xFF000000) >> 24)*nMult*2.0f - 1.0f;

  const float s6x = float((data.w & 0x000000FF)     )*nMult*2.0f - 1.0f;
  const float s6y = float((data.w & 0x0000FF00) >> 8)*nMult*2.0f - 1.0f;
  const float s7x = float((data.w & 0x00FF0000) >> 16)*nMult*2.0f - 1.0f;
  const float s7y = float((data.w & 0xFF000000) >> 24)*nMult*2.0f - 1.0f;
  
  const vec2 off0 = vec2(s0x,s0y)*(params.shadowMapSizeInv);
  const vec2 off1 = vec2(s1x,s1y)*(params.shadowMapSizeInv);
  const vec2 off2 = vec2(s2x,s2y)*(params.shadowMapSizeInv);
  const vec2 off3 = vec2(s3x,s3y)*(params.shadowMapSizeInv);

  const vec2 off4 = vec2(s4x,s4y)*(params.shadowMapSizeInv);
  const vec2 off5 = vec2(s5x,s5y)*(params.shadowMapSizeInv);
  const vec2 off6 = vec2(s6x,s6y)*(params.shadowMapSizeInv);
  const vec2 off7 = vec2(s7x,s7y)*(params.shadowMapSizeInv);
   
  /*
  const vec2 off0 = vec2(-0.9f,-0.8f)*(params.shadowMapSizeInv);
  const vec2 off1 = vec2(+0.8f,-0.9f)*(params.shadowMapSizeInv);
  const vec2 off2 = vec2(+0.8f,+1.0f)*(params.shadowMapSizeInv);
  const vec2 off3 = vec2(+1.0f,-0.9f)*(params.shadowMapSizeInv);

  const vec2 off4 = vec2(0.0f,   0.0f) *(params.shadowMapSizeInv);
  const vec2 off5 = vec2(-0.5f, -0.25f)*(params.shadowMapSizeInv);
  const vec2 off6 = vec2(+0.25f,-0.5f) *(params.shadowMapSizeInv);
  const vec2 off7 = vec2(+0.5f, +0.5f) *(params.shadowMapSizeInv);
  */

  const float shadow0    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off0, 0).x + 0.001f) ? 1.0f : 0.0f; 
  const float shadow1    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off1, 0).x + 0.001f) ? 1.0f : 0.0f; 
  const float shadow2    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off2, 0).x + 0.001f) ? 1.0f : 0.0f; 
  const float shadow3    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off3, 0).x + 0.001f) ? 1.0f : 0.0f; 

  const float shadow4    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off4, 0).x + 0.001f) ? 1.0f : 0.0f; 
  const float shadow5    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off5, 0).x + 0.001f) ? 1.0f : 0.0f; 
  const float shadow6    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off6, 0).x + 0.001f) ? 1.0f : 0.0f; 
  const float shadow7    = (posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord + off7, 0).x + 0.001f) ? 1.0f : 0.0f; 

  //const bool  outOfView = (shadowTexCoord.x < 0.001f || shadowTexCoord.x > 0.999f || shadowTexCoord.y < 0.001f || shadowTexCoord.y > 0.999f);
  //const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;  

  const float shadow = (1.0f/8.0f)*((shadow0 + shadow1 + shadow2 + shadow3) + (shadow4 + shadow5 + shadow6 + shadow7));

  const float dpFactor = max(dot(params.lightDir.xyz, surf.wNorm), 0.0f);
  const float cosPower = 120.0f;
  const vec3  v        = normalize(surf.wPos - params.wCamPos.xyz);
  const vec3  r        = reflect(v, surf.wNorm);
  const float cosAlpha = clamp(dot(params.lightDir.xyz, r), 0.0f, 1.0f);

  out_fragColor = texture(diffColor, surf.texCoord*2.0f)*(dpFactor*shadow + 0.25f) + vec4(1,1,1,1)*pow(cosAlpha, cosPower)*dpFactor*shadow;
}