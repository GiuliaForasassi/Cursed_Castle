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
    // Normalize the fragment texture coordinate to get the direction vector
    vec3 dir = normalize(fragTexCoord);

    // Map the 3D direction vector to 2D equirectangular UV coordinates
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
    vec2 skyUV = vec2(u, v);

    // Sample the sky texture
    vec3 baseColor = textureLod(skyMap, skyUV, 0.0).rgb;

    // 1. Cursed / Night appearance (very dark, desaturated, and tinted with spectral purple)
    float gray = dot(baseColor, vec3(0.299, 0.587, 0.114));
    vec3 desaturated = mix(baseColor, vec3(gray), 0.8);
    vec3 cursedColor = desaturated * vec3(0.12, 0.07, 0.22);

    // 2. Day appearance (natural warm tint)
    vec3 dayColor = baseColor * vec3(1.02, 1.0, 0.98);

    // 3. Smooth interpolation between Cursed and Day
    float t = clamp(ubo.dayFactor, 0.0, 1.0);
    vec3 finalColor = mix(cursedColor, dayColor, t);

    // The sky is a light source: scale it to HDR like other lights before tone mapping
    finalColor *= mix(0.9, 3.0, t);
    finalColor = finalColor / (finalColor + vec3(1.0));

    outColor = vec4(finalColor, 1.0);
}