#version 450
#extension GL_ARB_separate_shader_objects : enable

// Uniform buffer for the view-projection matrix of the SkyBox
layout(binding = 0, set = 0) uniform SkyBoxUniformBlock {
    mat4 mvpMat;        // Prj * ViewNoTranslation
    float dayFactor;    // 0.0 = Notte, 1.0 = Giorno
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoord; // Direzione vettoriale per il campionamento della sfera/cubo

void main() {
    // The sampling direction corresponds to the original position of the cube vertex
    fragTexCoord = inPosition;

    // Trick z = w (.xyww): force the NDC depth to 1.0 (the farthest possible plane)
    vec4 pos = ubo.mvpMat * vec4(inPosition, 1.0);
    // Clipping: avoid cubo che taglia la mappa
    gl_Position = vec4(pos.xy, pos.w, pos.w);
}