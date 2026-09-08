//---------- SHADER FOR RELICS, ALTARS, STATUES AND METALS -------

#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_POINT_LIGHTS 16

//--------------- Input and output definitions ---------------
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) flat in vec4 matParams;
layout(location = 3) in vec3 fragNormal; // Fragment normal vector


layout(location = 0) out vec4 outColor;

// Texture 2D for Descriptor Set 1
layout(binding = 1, set = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 1) uniform sampler2D shadowMap; // Shadow map for shadow mapping

// Global Uniform Buffer (Descriptor Set 0)
layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
    vec3 lightDir;
    vec4 lightColor;
    vec3 eyePos;

    vec4 pointLightPos[MAX_POINT_LIGHTS];
    vec4 pointLightColor[MAX_POINT_LIGHTS];
    vec4 fogColor; 
    mat4 lightVP; // Light view-projection matrix for shadow mapping
} gubo;

const float PI = 3.14159265359;

// 1. Distribuzione delle microfaccette (Normal Distribution Function: GGX / Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

// 2. Geometry Function / Self-shading (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 0.0000001);
}

// Smith Geometry Function (combines view and light geometry)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 3. Fresnel Equation (Fresnel-Schlick)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cook-Torrance BRDF calculation for a single light source
vec3 computeCookTorrance(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float roughness, float metallic, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0)
        return vec3(0.0);

    // Cook-Torrance BRDF components
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;
    vec3 specular = numerator / max(denominator, 0.0001);

    // Conservazione dell'energia: kD (diffuso) + kS (speculare) = 1.0
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic); // I metalli puri non hanno riflessione diffusa

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// Computes the visibility of a fragment with respect to the directional light, taking into account shadows. Returns 1.0 if fully visible, 0.0 if fully in shadow
// It takes a point on the scene and its normal, projects it into the light space and returns the percentage direct light that reaches it (1.0 = fully visible, 0.0 = fully in shadow)
float directionalVisibility(vec3 worldPosition, vec3 normal) {

    vec4 lightClip = gubo.lightVP * vec4(worldPosition, 1.0);
    // Transform into normalized device coordinates (NDC)
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    // Converts from NDC space to texture coordinates (UV: 0.0 to 1.0)
    vec2 shadowUV = lightNdc.xy * 0.5 + 0.5;

    // If the fragment is outside the light's view frustum or outside the shadow map, consider it fully visible
    if (lightNdc.z < 0.0 || lightNdc.z > 1.0 ||
        any(lessThan(shadowUV, vec2(0.0))) ||
        any(greaterThan(shadowUV, vec2(1.0)))) {
        return 1.0;
    }
    // Bias to prevent shadow acne
    vec3 toLight = normalize(-gubo.lightDir);
    float bias = max(
        0.002 * (1.0 - max(dot(normal, toLight), 0.0)),
        0.0007
    );

    // Percentage-closer filtering (PCF) for soft shadows
    // 
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    // Depth of the receiver fragment in light space, adjusted by bias
    float receiverDepth = lightNdc.z - bias;
    float visibility = 0.0;

    // Iterate over the 3x3 neighborhood of the shadow map to perform PCF
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            vec2 sampleUV = shadowUV +
                vec2(float(offsetX), float(offsetY)) * texelSize;

            if (any(lessThan(sampleUV, vec2(0.0))) ||
                any(greaterThan(sampleUV, vec2(1.0)))) {
                visibility += 1.0;
            } else {
                float closestDepth =
                    textureLod(shadowMap, sampleUV, 0.0).r;
                visibility += receiverDepth <= closestDepth ? 1.0 : 0.0;
            }
        }
    }

return visibility / 9.0;
}

void main() {
    // Calulate the fragment normal vector in world space
    vec3 N = normalize(fragNormal);

    // Albedo lineare
    vec3 albedo = texture(albedoMap, fragUV).rgb;
    vec3 V = normalize(gubo.eyePos - fragPos);

    // Extract roughness and metallic values from material parameters
    float roughness = clamp(matParams.w, 0.05, 1.0);
    float metallic = clamp(matParams.z, 0.0, 1.0);

    // Base reflectance F0 (0.04 for dielectrics, albedo for metals)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // 1. Directional Light (Sun / Moon)
    vec3 L = normalize(-gubo.lightDir);
    vec3 Lo = computeCookTorrance(N, V, L, gubo.lightColor.rgb, albedo, roughness, metallic, F0) * matParams.x;
    Lo *= directionalVisibility(fragPos, N);

    // 2. Point Lights (Torches)
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if (gubo.pointLightColor[i].a <= 0.01) 
            continue;

        vec3 toLight = gubo.pointLightPos[i].xyz - fragPos;
        float distance = length(toLight);
        vec3 Lp = toLight / max(distance, 0.0001);

        float g = gubo.pointLightPos[i].w;
        float attenuation = min(pow(g / max(distance, 0.001), 2.0), 1.0);
        attenuation *= clamp(1.0 - pow(distance / (3.0 * g), 4.0), 0.0, 1.0);
        vec3 radianceP = gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation);

        Lo += computeCookTorrance(N, V, Lp, radianceP, albedo, roughness, metallic, F0);
    }

    // 3. Ambient Light
    vec3 ambient = mix(0.02, 0.03, matParams.x) * albedo;
    vec3 emissive = matParams.y * albedo * vec3(2.0, 1.2, 0.5);
    vec3 color = ambient + Lo + emissive;

    // 4. Dynamic Fog
    if (gubo.fogColor.a > 0.0001) {
        float distToEye = length(gubo.eyePos - fragPos);
        float fogFactor = clamp(exp(-gubo.fogColor.a * distToEye), 0.0, 1.0);
        color = mix(gubo.fogColor.rgb, color, fogFactor);
    }

    // 5. Reinhard Tone Mapping
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}