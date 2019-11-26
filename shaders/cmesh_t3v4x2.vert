#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
  mat4 mWorldView;
  mat4 mProj;  

} params;

void main(void)
{ 
  const vec4 posCamSpace = params.mWorldView*vec4(vPosNorm.xyz, 1.0);

  gl_Position   = params.mProj*posCamSpace;
  gl_Position.y = -gl_Position.y;	// Vulkan coordinate system is different to OpenGL    
}
