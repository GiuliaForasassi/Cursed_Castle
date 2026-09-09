#version 450

layout(location = 0) in vec3 inPosition;
layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 mvpMat;
    mat4 mMat;
    vec4 lightParams;
} ubo;
layout(push_constant) uniform ShadowPush {
    mat4 lightVP;
} shadow;
void main() {
    gl_Position = shadow.lightVP * ubo.mMat * vec4(inPosition, 1.0);
}