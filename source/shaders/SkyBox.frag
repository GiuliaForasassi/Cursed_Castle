#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Uniform buffer con il fattore di transizione
layout(binding = 0, set = 0) uniform SkyBoxUniformBlock {
    mat4 mvpMat;
    float dayFactor; // 0.0 = Notte maledetta, 1.0 = Giorno sereno
} ubo;

// 1 sola Texture equirettangolare del cielo (Set 0, Binding 1)
layout(binding = 1, set = 0) uniform sampler2D skyMap;

const float PI = 3.141592653589793;

void main() {
    // Normalizza la direzione dal centro del cubo/sfera
    vec3 dir = normalize(fragTexCoord);

    // Mappatura sferica equirettangolare 3D -> UV 2D
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
    vec2 skyUV = vec2(u, v);

    // Campionamento del cielo
    vec3 baseColor = pow(texture(skyMap, skyUV).rgb, vec3(2.2));

    // 1. Aspetto Cursed / Notturno (desaturato e tinto di blu/viola spettrale)
    float gray = dot(baseColor, vec3(0.299, 0.587, 0.114));
    vec3 desaturated = mix(baseColor, vec3(gray), 0.65);
    vec3 cursedColor = desaturated * vec3(0.35, 0.28, 0.55);

    // 2. Aspetto Giorno sereno (tint caldo naturale)
    vec3 dayColor = baseColor * vec3(1.02, 1.0, 0.98);

    // 3. Interpolazione fluida tra Cursed e Giorno
    float t = clamp(ubo.dayFactor, 0.0, 1.0);
    vec3 finalColor = mix(cursedColor, dayColor, t);

    // Gamma correction finale per sRGB
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    outColor = vec4(finalColor, 1.0);
}