//---------- SHADER FOR ROCKS, WALLS, WOOD AND GROUND -------

#version 450 // GLSL version 4.5
#extension GL_ARB_separate_shader_objects : enable // Enable separate shader objects for modular shader programming

#define MAX_POINT_LIGHTS 20 // Number of point lights
#define POINT_SHADOW_LIGHTS 20 // Number of point light shadow-casting lights

// Fragment shader for Blinn-Phong lighting model with support for directional and point lights

//--------------- Inputs: attributes of the fragment (from vertex shader) ---------------
layout(location = 0) in vec3 fragPos; // Fragment position (world space)
layout(location = 1) in vec2 fragUV; // Texture coordinates
layout(location = 2) flat in vec4 matParams;  // x = outdoor flag (1.0 = receives directional light), y = emissive intensity (z,w unused here)
layout(location = 3) in vec3 fragNormal; // Normal (world space)

//--------------- Output ---------
layout(location = 0) out vec4 outColor; // Final color pixel

//------------- Variables ---------------
layout(binding = 1, set = 1) uniform sampler2D albedoMap; // Texture containing the base color (albedo) of the object
layout(set = 0, binding = 1) uniform sampler2D shadowMap; // Shadow map for shadow mapping
layout(set = 0, binding = 2) uniform sampler2D pointShadowAtlas; // Shadow atlas for point light shadows

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

    mat4 pointShadowVP[6 * POINT_SHADOW_LIGHTS]; // View-projection matrices for the point light's shadow cubemap faces
} gubo;

// --------------- Directional Light Shadow Visibility ---------------
// Computes the visibility of a fragment with respect to the directional light, taking into account shadows. Returns 1.0 if fully visible, 0.0 if fully in shadow.
// It takes a point on the scene and its normal, projects it into the light space and returns the percentage direct light that reaches it (1.0 = fully visible, 0.0 = fully in shadow)
float directionalVisibility(vec3 worldPosition) {
    vec4 lightClip = gubo.lightVP * vec4(worldPosition, 1.0);
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec3 shadowPosition = vec3(lightNdc.xy * 0.5 + 0.5, lightNdc.z);

    vec3 derivativeX = dFdx(shadowPosition);
    vec3 derivativeY = dFdy(shadowPosition);
    vec3 receiverPlane = cross(derivativeX, derivativeY);
    vec2 depthGradient = vec2(0.0);
    if (abs(receiverPlane.z) > 0.000001 * length(receiverPlane)) {
        depthGradient = -receiverPlane.xy / receiverPlane.z;
    }

    if (any(lessThan(shadowPosition, vec3(0.0))) ||
        any(greaterThan(shadowPosition, vec3(1.0)))) {
        return 1.0;
    }

    ivec2 mapSize = textureSize(shadowMap, 0);
    ivec2 centerPixel = ivec2(floor(shadowPosition.xy * vec2(mapSize)));
    float bias = 0.01 / (400.0 - 1.0);
    float visibility = 0.0;

    const int filterRadius = 2;

    for (int offsetY = -filterRadius; offsetY <= filterRadius; ++offsetY) {
        for (int offsetX = -filterRadius; offsetX <= filterRadius; ++offsetX) {
            ivec2 samplePixel = centerPixel + ivec2(offsetX, offsetY);
            if (any(lessThan(samplePixel, ivec2(0))) ||
                any(greaterThanEqual(samplePixel, mapSize))) {
                continue;
            }
            vec2 sampleUV = (vec2(samplePixel) + vec2(0.5)) / vec2(mapSize);
            float receiverDepth = shadowPosition.z
                + dot(depthGradient, sampleUV - shadowPosition.xy) - bias;
            float storedDepth = texelFetch(shadowMap, samplePixel, 0).r;
            visibility += receiverDepth <= storedDepth ? 1.0 : 0.0;
        }
    }
    float sampleCount = float((2 * filterRadius + 1) * (2 * filterRadius + 1));
    return visibility / sampleCount;
}

// ------------- Point Light Shadow Visibility -------------
// Function that determines whether a point in the scene is in shadow relative to the torch or not
float pointVisibility(vec3 worldPosition, vec3 normal, int lightIndex) {
    // Compute the vector from the point light to the fragment and its distance
    vec3 fromLight = worldPosition - gubo.pointLightPos[lightIndex].xyz;
    float lightDistance = length(fromLight);
    if (lightDistance < 0.0001) // If the fragment is extremely close to the light, consider it fully lit
        return 1.0;

    // Find the component of the vector that has the largest absolute value
    // This indicates in which principal axis the vector is most aligned, which determines the corresponding face of the cubemap
    vec3 absoluteDirection = abs(fromLight);
    int face;
    if (absoluteDirection.x >= absoluteDirection.y &&
        absoluteDirection.x >= absoluteDirection.z) {
        if (fromLight.x >= 0.0) {
            face = 0;
        } else {
            face = 1;
        }
    } else if (absoluteDirection.y >= absoluteDirection.z) {
        if (fromLight.y >= 0.0) {
            face = 2;
        } else {
            face = 3;
        }
    } else {
        if (fromLight.z >= 0.0) {
            face = 4;
        } else {
            face = 5;
        }
    }
    // Transform the world position of the fragment into the light's clip space for the selected cubemap face
    vec4 clip = gubo.pointShadowVP[lightIndex * 6 + face] * vec4(worldPosition, 1.0);
    if (clip.w <= 0.0)
        return 1.0; // If the fragment is behind the near plane of the light's view, consider it fully lit
    vec3 ndc = clip.xyz / clip.w; // Convert the clip space coordinates to normalized device coordinates (NDC)
    if (ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0;

    // Compute the direction to the light and apply a bias to avoid shadow acne
    vec3 toLight = -fromLight / lightDistance;
    float normalDotLight = clamp(dot(normal, toLight), 0.0, 1.0);
    float slope = sqrt(max(1.0 - normalDotLight * normalDotLight, 0.0))/ max(normalDotLight, 0.1);
    float faceResolution = float(textureSize(pointShadowAtlas, 0).x) / 3.0;
    float worldTexelEstimate = 2.0 * lightDistance / faceResolution;

    const float filterFootprint = 2.5;
    float bias = max(0.04 + 0.08 * (1.0 - normalDotLight),filterFootprint *worldTexelEstimate * slope);

    vec3 biasedPosition = worldPosition + toLight * min(bias, lightDistance * 0.5);
    vec4 biasedClip = gubo.pointShadowVP[lightIndex * 6 + face] * vec4(biasedPosition, 1.0);
    float receiverDepth = biasedClip.z / biasedClip.w;

    ivec2 faceSize = textureSize(pointShadowAtlas, 0) / ivec2(3, 2 * POINT_SHADOW_LIGHTS);
    vec2 faceUV = ndc.xy * 0.5 + 0.5;
    ivec2 pixel = clamp(
        ivec2(floor(faceUV * vec2(faceSize))),
        ivec2(0), faceSize - ivec2(1));
    ivec2 tileOrigin = ivec2(face % 3, lightIndex * 2 + face / 3) * faceSize;

    float visibility = 0.0;

    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            ivec2 samplePixel = clamp(
                pixel + ivec2(offsetX, offsetY),
                ivec2(0), faceSize - ivec2(1));

            float storedDepth = texelFetch(
                pointShadowAtlas, tileOrigin + samplePixel, 0).r;

            visibility += receiverDepth <= storedDepth ? 1.0 : 0.0;
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
    // Computed once and reused for both the direct light and the sky ambient:
    // the same occluders that block the sun also occlude the sky dome.
    float sunVisibility = matParams.x > 0.0 ? directionalVisibility(fragPos) : 0.0;
    vec3 Lo = (albedo + vec3(pow(HdotN, 128.0)) * 0.04) * NdotL * radianceDir * matParams.x;
    Lo *= sunVisibility;

    //--------------- Point light calculations ---------------
    vec3 bounce = vec3(0.0); // Accumulator for the fake indirect bounce (torch light reflected by the walls)
    for(int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if(gubo.pointLightColor[i].a <= 0.01) // Alpha < 0.01: skip inactive point lights
            continue; 

        vec3 toLight = gubo.pointLightPos[i].xyz - fragPos; // Vector from fragment to point light
        float distance = length(toLight);
        vec3 Lp = toLight / max(distance, 0.0001); // Direction from fragment to point light
        vec3 Hp = normalize(V + Lp); // Half-vector

        float g = gubo.pointLightPos[i].w; // Range of the point light
        float attenuation = min(pow(g / max(distance, 0.001), 2.0), 1.0); // Quadratic attenuation
        attenuation *= clamp(1.0 - pow(distance / (3.0 * g), 4.0), 0.0, 1.0); // Roll-off factor
        if (attenuation <= 0.0)
            continue;

        vec3 radiancePoint = gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation);
        bounce += radiancePoint; // Pre-shadow, pre-NdotL: reaches back-facing surfaces too

        float NdotLp = max(dot(N, Lp), 0.0);
        if (NdotLp <= 0.0)
            continue; // Direct light only for front-facing surfaces

        radiancePoint *= pointVisibility(fragPos, N, i);

        float HdotNp = max(dot(Hp, N), 0.0);
        Lo += (albedo + vec3(pow(HdotNp, 64.0)) * 0.03) * NdotLp * radiancePoint;
    }
        
    //---------- Compute the ambient component of the lighting ------------
    // 5. Hemisphere ambient: the ambient term is no longer a flat constant,
    // but a gradient between the sky color (for up-facing surfaces, N.y > 0)
    // and a warm ground-bounce color (for down-facing surfaces, N.y < 0).
    // This gives unlit walls/ceilings a non-uniform, more realistic shading.
    const float indoorAmbientStrength = 0.5;
    float hemi = 0.5 + 0.5 * N.y; // 1.0 = surface facing the sky, 0.0 = facing the ground

    // Outdoor: cool sky tint from above, warm dirt-bounce tint from below
    const vec3 skyTint = vec3(0.70, 0.80, 1.00); // cool bluish sky light
    const vec3 groundTint = vec3(0.55, 0.45, 0.35); // warm bounce from the ground

    // Indoor: dim ambient, slightly cooler on up-facing surfaces (light falling
    // from the ceiling area) and warmer on down-facing ones (bounce from the floor)
    const vec3 indoorUpColor = vec3(0.065, 0.062, 0.055); // cool-ish stone bounce
    const vec3 indoorDownColor = vec3(0.032, 0.027, 0.022); // dark warm crevice color

    // The sky ambient must only reach surfaces that actually "see" the sky.
    // Walls are outdoor-classified (their exterior must be sunlit), but their
    // interior faces are occluded from the sky: the directional shadow map is
    // a cheap proxy for sky visibility, so interior faces fall back to the
    // dark indoor ambient. Reuses sunVisibility computed above (no second
    // shadow-map traversal).
    float skyOcclusion = mix(0.35, 1.0, sunVisibility);
    vec3 skyAmbient = (indoorAmbientStrength + 0.015 * max(gubo.lightColor.r, gubo.lightColor.b))
        * mix(groundTint, skyTint, hemi) * albedo * skyOcclusion;
    vec3 indoorAmbient = mix(indoorDownColor, indoorUpColor, hemi) * albedo;
    vec3 ambient = mix(indoorAmbient, skyAmbient, matParams.x);
    // La fiamma della torcia si illumina da sola: non dipende dalle sorgenti
    vec3 emissive = matParams.y * albedo * vec3(2.0, 1.2, 0.5);
    // Fake indirect illumination: torch light bouncing off the surrounding
    // surfaces reaches even shadowed areas. It is unshadowed and ignores
    // NdotL (diffuse interreflection is roughly view/normal independent),
    // so it fills the shadows with a soft, warm, non-uniform glow.
    vec3 bounceFill = bounce * albedo * 0.07;
    vec3 color = ambient + Lo + emissive + bounceFill;

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
