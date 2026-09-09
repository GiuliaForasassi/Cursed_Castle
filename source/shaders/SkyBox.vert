#version 450
#extension GL_ARB_separate_shader_objects : enable

// Uniform buffer per la matrice di vista-proiezione dello SkyBox
layout(binding = 0, set = 0) uniform SkyBoxUniformBlock {
    mat4 mvpMat;        // Prj * ViewNoTranslation
    float dayFactor;    // 0.0 = Notte, 1.0 = Giorno
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoord; // Direzione vettoriale per il campionamento della sfera/cubo

void main() {
    // La direzione di campionamento corrisponde alla posizione originale del vertice del cubo
    fragTexCoord = inPosition;

    // Trucco z = w (.xyww): forza la profondità NDC a 1.0 (il piano più lontano possibile)
    vec4 pos = ubo.mvpMat * vec4(inPosition, 1.0);
    gl_Position = vec4(pos.xy, pos.w, pos.w);
}