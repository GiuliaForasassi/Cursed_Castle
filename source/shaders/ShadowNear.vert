#version 450

#define MAX_POINT_LIGHTS 20
#define POINT_SHADOW_LIGHTS 20

layout(location = 0) in vec3 inPosition;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 mvpMat;
    mat4 mMat;
    vec4 lightParams;
} ubo;

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {
    vec3 lightDir;
    vec4 lightColor;
    vec3 eyePos;
    vec4 pointLightPos[MAX_POINT_LIGHTS];
    vec4 pointLightColor[MAX_POINT_LIGHTS];
    vec4 fogColor;
    mat4 lightVP;
    mat4 pointShadowVP[6 * POINT_SHADOW_LIGHTS];
    mat4 lightVPFar;
} gubo;

void main() {
    gl_Position = gubo.lightVP * ubo.mMat * vec4(inPosition, 1.0);
}