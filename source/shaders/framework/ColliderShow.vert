#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 0) uniform UniformBufferObject {
	mat4 vpMat;
	vec4 strokeColors[20];   // strokeColors[MAX_COLLIDERS]
} ubo;

layout(push_constant) uniform PushConsts {
	int colliderIndex;
} pushConsts;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 color;  // Pass the color to Fragment Shader

void main() {
	gl_Position = ubo.vpMat * vec4(inPosition, 1.0);

	// Get shown collider's color
	color = ubo.strokeColors[pushConsts.colliderIndex];
}
