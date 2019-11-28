#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
  vec3 testColor;

} surf;

void main()
{
  const vec3  lightDir = normalize(vec3(1,1,1));
  const float dpFactor = dot(lightDir, surf.wNorm);

  color = vec4(1.0f, 1.0f, 1.0f, 1.0f)*dpFactor;
}