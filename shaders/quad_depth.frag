#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(push_constant) uniform params_t
{
  vec4 scaleAndOffs;
  vec4 depthMinMaxScale;

} params;

void main()
{
  const float depth = (textureLod(colorTex, surf.texCoord, 0).x - params.depthMinMaxScale.x)*params.depthMinMaxScale.z;
  color = vec4(depth,depth,depth,1);
}
