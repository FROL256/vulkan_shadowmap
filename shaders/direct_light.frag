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

layout(push_constant) uniform params_t
{
  mat4 mWorldView;
  mat4 mProj;  
  vec4 wCamPos;
  vec4 lightDir;

} params;

void main()
{
  const float dpFactor = max(dot(params.lightDir.xyz, surf.wNorm), 0.0f);

  const float cosPower = 120.0f;
  const vec3  v        = normalize(surf.wPos - params.wCamPos.xyz);
  const vec3  r        = reflect(v, surf.wNorm);
  const float cosAlpha = clamp(dot(params.lightDir.xyz, r), 0.0f, 1.0f);

  out_fragColor = texture(diffColor, surf.texCoord*2.0f)*(dpFactor + 0.05f) + vec4(1,1,1,1)*pow(cosAlpha, cosPower);
}