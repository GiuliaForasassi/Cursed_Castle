#version 450 // GLSL version for Vulkan (4.5)
#extension GL_ARB_separate_shader_objects : enable // Enable separated compilation of shader stages

layout(binding = 0, set = 1) uniform UniformBufferObject {
	mat4 mvpMat; // Matrix model view-projection
	mat4 mMat; // Matrix model (local transformation of the object in the world)
	vec4 lightParams; // x = 1.0 outdoor (receives the directional light), 0.0 inside the castle
} ubo;

//----- Input: vertex attributes -----
layout(location = 0) in vec3 inPosition; // Position (model space)
layout(location = 1) in vec2 inUV; // Texture coordinates
layout(location = 2) in vec3 inNormal; // Normal (model space)

//----- Output: attributes to the fragment shader -----
layout(location = 0) out vec3 fragPos; // Fragment position (world space)
layout(location = 1) out vec2 fragUV; // Texture coordinates
layout(location = 2) flat out vec4 matParams; // Material parameters (flat: not interpolated across the primitive)
layout(location = 3) out vec3 fragNormal; // Normal (world space)

void main() {
	// Determine where to put the vertex on the screen
	gl_Position = ubo.mvpMat * vec4(inPosition, 1.0); 
	// Calculate the fragment position in 3d world space
	fragPos = (ubo.mMat * vec4(inPosition, 1.0)).xyz;
	// Pass the texture coordinates to the fragment shader
	fragUV  = inUV;
	// Pass the material parameters to the fragment shader
	matParams = ubo.lightParams;
	// Calculate the fragment normal vector in world space 
	fragNormal = transpose(inverse(mat3(ubo.mMat))) * inNormal;
}
