#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;

} surf;

layout (binding = 0) uniform sampler2D diffColor;

void main()
{
  const vec3  lightDir  = normalize(vec3(1,1,1));
  const float dpFactor  = max(dot(lightDir, surf.wNorm), 0.0f);

  color = texture(diffColor, surf.texCoord)*(dpFactor + 0.05f);
}