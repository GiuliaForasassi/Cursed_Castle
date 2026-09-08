//---------- SHADER FOR ROCKS, WALLS, WOOD AND GROUND -------

#version 450 // GLSL version 4.5
#extension GL_ARB_separate_shader_objects : enable // Enable separate shader objects for modular shader programming

#define MAX_POINT_LIGHTS 16 // Number of point lights

// Fragment shader for Blinn-Phong lighting model with support for directional and point lights

//--------------- Inputs: attributes of the fragment (from vertex shader) ---------------
layout(location = 0) in vec3 fragPos; // Fragment position (world space)
layout(location = 1) in vec2 fragUV; // Texture coordinates
layout(location = 2) flat in vec4 matParams; // Material parameters: x = direction, y = emissive intensity, z = metallic, w = roughness
layout(location = 3) in vec3 fragNormal; // Normal (world space)

//--------------- Output ---------
layout(location = 0) out vec4 outColor; // Final color pixel

//------------- Variables ---------------
layout(binding = 1, set = 1) uniform sampler2D albedoMap; // Texture containing the base color (albedo) of the object
layout(set = 0, binding = 1) uniform sampler2D shadowMap; // Shadow map for shadow mapping

//UNIFORM SET 0: Global parameters (directional light, camera position, point lights, fog)
layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
    // -------- Directional light parameters --------
    vec3 lightDir; // Direction of the main directional light in world space
    vec4 lightColor; // Color of the main directional light (RGB) and intensity (A)
    vec3 eyePos; // Position of the camera in world space

    // -------- Point light parameters --------
    vec4 pointLightPos[MAX_POINT_LIGHTS]; // Position (xyz) and range (w) of the point lights
    vec4 pointLightColor[MAX_POINT_LIGHTS]; // Color (RGB) and intensity (A) of the point lights

    vec4 fogColor; // Fog color for atmospheric effects: RGB + a = density (0.0 = no fog, 1.0 = full fog)
    mat4 lightVP; // Light view-projection matrix for shadow mapping
} gubo;

const float PI = 3.14159265359;

// Computes the visibility of a fragment with respect to the directional light, taking into account shadows. Returns 1.0 if fully visible, 0.0 if fully in shadow.
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
    
    // 1. Compute the normal vector
	vec3 N = normalize(fragNormal);
    // 2. Fetch the albedo color from the texture
    vec3 albedo = texture(albedoMap, fragUV).rgb;

    //--------------- Directional light calculations ---------------
    // 3. Compute view direction vector (from fragment position to camera position)
    vec3 V = normalize(gubo.eyePos - fragPos); // Direction from fragment to camera
    vec3 L = normalize(-gubo.lightDir); // Direction from fragment to directional light
    vec3 H = normalize(V + L); // Half-vector between view and light directions
    vec3 radianceDir = gubo.lightColor.rgb;

    // 4. Compute the dot products for the diffuse and specular components
    float NdotL = max(dot(N, L), 0.0);
    float HdotN = max(dot(H, N), 0.0);
    vec3 Lo = (albedo + vec3(pow(HdotN, 128.0)) * 0.04) * NdotL * radianceDir * matParams.x;
    Lo *= directionalVisibility(fragPos, N);

    //--------------- Point light calculations ---------------
    for(int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if(gubo.pointLightColor[i].a <= 0.01) // Alpha < 0.01: skip inactive point lights
            continue; 

        vec3 toLight = gubo.pointLightPos[i].xyz - fragPos; // Vector from fragment to point light
        float distance = length(toLight);
        vec3 Lp = toLight / max(distance, 0.0001); // Direction from fragment to point light
        vec3 Hp = normalize(V + Lp); // Half-vector

        float g = gubo.pointLightPos[i].w; // Range of the point light
        float attenuation = min(pow(g / max(distance, 0.001), 2.0), 1.0); // Quadratic attenuation based on distance and range of the point light
        attenuation *= clamp(1.0 - pow(distance / (3.0 * g), 4.0), 0.0, 1.0); // Multiply the attenuation by a roll-off factor
        vec3 radiancePoint = gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation); // Radiance of the point light after attenuation

        float NdotLp = max(dot(N, Lp), 0.0); // Scalar product of normal and light direction for diffuse component
        float HdotNp = max(dot(Hp, N), 0.0); // Scalar product of half-vector and normal for specular component
        vec3 LoPoint = (albedo + vec3(pow(HdotNp, 64.0)) * 0.03) * NdotLp * radiancePoint; // Total contribution of the point light to the fragment color
        Lo += LoPoint;
    }
        
    //---------- Compute the ambient component of the lighting ------------
    // 5. Apply a small ambient term (0.015) to simulate indirect lighting
    vec3 skyAmbient = (0.025 + 0.015 * max(gubo.lightColor.r, gubo.lightColor.b)) * albedo;
    vec3 ambient = mix(0.02 * albedo, skyAmbient, matParams.x);
    // La fiamma della torcia si illumina da sola: non dipende dalle sorgenti
    vec3 emissive = matParams.y * albedo * vec3(2.0, 1.2, 0.5);
    vec3 color = ambient + Lo + emissive;

    // 6. Apply exponential fog (enabled if fogColor.a > 0)
    if (gubo.fogColor.a > 0.0001) {
        float distToEye = length(gubo.eyePos - fragPos);
        float fogFactor = clamp(exp(-gubo.fogColor.a * distToEye), 0.0, 1.0);
        color = mix(gubo.fogColor.rgb, color, fogFactor);
    }

    // 7. Reinhard tone mapping to compress values of final color HDR that exceed the displayable range
    color = color / (color + vec3(1.0));

    // 8. Output the final color of the fragment
    outColor = vec4(color, 1.0);
}
