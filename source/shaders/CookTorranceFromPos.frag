//---------- SHADER FOR RELICS, ALTARS, STATUES AND METALS -------

#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_POINT_LIGHTS 22 // Number of point lights
#define POINT_SHADOW_LIGHTS 22 // Number of point light shadow-casting lights

//--------------- Input and output definitions ---------------
layout(location = 0) in vec3 fragPos; // Fragment position (world space)
layout(location = 1) in vec2 fragUV; // Fragment texture coordinates
layout(location = 2) flat in vec4 matParams;
layout(location = 3) in vec3 fragNormal; // Fragment normal vector


layout(location = 0) out vec4 outColor; // Output color of the pixel

// Texture 2D for Descriptor Set 1
layout(binding = 1, set = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 1) uniform sampler2D shadowMap; // Shadow map for shadow mapping
layout(set = 0, binding = 2) uniform sampler2D pointShadowAtlas; // Shadow atlas for point light shadows

// Global Uniform Buffer (Descriptor Set 0)
layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
    vec3 lightDir; // Direction of the main directional light in world space
    vec4 lightColor; // Color and intensity of the main directional light
    vec3 eyePos;

    vec4 pointLightPos[MAX_POINT_LIGHTS]; // Position (xyz) and range (w) of the point lights
    vec4 pointLightColor[MAX_POINT_LIGHTS]; // Color (RGB) and intensity (A) of the point lights
    vec4 fogColor; // Color of the fog
    mat4 lightVP; // Light view-projection matrix for directional light shadow mapping

    mat4 pointShadowVP[6 * POINT_SHADOW_LIGHTS]; // View-projection matrices for the point light's shadow cubemap faces
} gubo;

const float PI = 3.14159265359;

// ---------------- Helper functions used to define the specular component ---------------
// 1. Distribution microfaccette
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0); // co
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
// Model the auto-shadow
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 3. Fresnel Equation (Fresnel-Schlick)
// How tha light is reflected based on observation angle
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

// --------------- Directional Light Shadow Visibility ---------------
// Computes the visibility of a fragment with respect to the directional light, taking into account shadows. Returns 1.0 if fully visible, 0.0 if fully in shadow.
// It takes a point on the scene and its normal, projects it into the light space and returns the percentage direct light that reaches it (1.0 = fully visible, 0.0 = fully in shadow)
float directionalVisibility(vec3 worldPosition, vec3 normal) {
    // Transform the world position into the light's clip space
    vec4 lightClip = gubo.lightVP * vec4(worldPosition, 1.0);
    // Perform perspective divide to get normalized device coordinates (NDC)
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    // Convert NDC to texture coordinates (rgb) for shadow mapping (0.0 to 1.0 range)
    vec3 shadowPosition = vec3(lightNdc.xy * 0.5 + 0.5, lightNdc.z);

    // Estimate how these coordinates change between nearby fragments on the screen
    vec3 derivativeX = dFdx(shadowPosition);
    vec3 derivativeY = dFdy(shadowPosition);
    // Compute the plane that receives the shadow based on the derivatives
    vec3 receiverPlane = cross(derivativeX, derivativeY);
    // Initialize the depth gradient for the shadow receiver plane
    vec2 depthGradient = vec2(0.0);
    // Compute the depth gradient only if the receiver plane is not nearly perpendicular to the view direction
    if (abs(receiverPlane.z) > 0.000001 * length(receiverPlane)) {
        //  Compute the depth gradients with respect to the UV coordinates
        depthGradient = -receiverPlane.xy / receiverPlane.z;
    }

    // If the fragment is outside the shadow map bounds, consider it fully lit
    if (any(lessThan(shadowPosition, vec3(0.0))) ||
        any(greaterThan(shadowPosition, vec3(1.0)))) {
        return 0.0;
    }
    // Retrieve the size of the shadow map and compute the center pixel for sampling
    ivec2 mapSize = textureSize(shadowMap, 0);
    ivec2 centerPixel = ivec2(floor(shadowPosition.xy * vec2(mapSize)));
    // Compute the bias for shadow acne prevention
    float bias = 0.01 / (400.0 - 1.0);
    // Initialize the visibility accumulator for the percentage-closer filtering (PCF) loop
    float visibility = 0.0;

    // Define the radius of the PCF filter kernel
    const int filterRadius = 2;

    // Perform the PCF loop to accumulate visibility from neighboring samples
    // 5x5: 25 samples will be considered for the PCF filter
    for (int offsetY = -filterRadius; offsetY <= filterRadius; ++offsetY) {
        for (int offsetX = -filterRadius; offsetX <= filterRadius; ++offsetX) {
            ivec2 samplePixel = centerPixel + ivec2(offsetX, offsetY);
            if (any(lessThan(samplePixel, ivec2(0))) ||
                any(greaterThanEqual(samplePixel, mapSize))) {
                continue;
            }
            // Convert the sample pixel coordinates to UV coordinates for texture sampling
            vec2 sampleUV = (vec2(samplePixel) + vec2(0.5)) / vec2(mapSize);
            float receiverDepth = shadowPosition.z
                + dot(depthGradient, sampleUV - shadowPosition.xy) - bias;
            float storedDepth = texelFetch(shadowMap, samplePixel, 0).r;
            visibility += receiverDepth <= storedDepth ? 1.0 : 0.0;
        }
    }
    // Compute the total number of samples considered in the PCF filter
    float sampleCount = float((2 * filterRadius + 1) * (2 * filterRadius + 1));
    // Return the average visibility as the final shadow factor
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
    // Transform the world position into the light's clip space using the appropriate face of the cubemap
    vec4 clip = gubo.pointShadowVP[lightIndex * 6 + face] * vec4(worldPosition, 1.0);
    // 
    if (clip.w <= 0.0)
        return 1.0; // 
    vec3 ndc = clip.xyz / clip.w; // Convert the clip space coordinates to normalized device coordinates (NDC)
    if (ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0;

    // Compute the normalized direction to the light 
    vec3 toLight = -fromLight / lightDistance;
    //float bias = 0.04 + 0.08 *(1.0 - max(dot(normal, toLight), 0.0));
    float normalDotLight = clamp(dot(normal, toLight), 0.0, 1.0);
    // Approximate the tangent of the angle between the surface normal and the light direction
    float slope = sqrt(max(1.0 - normalDotLight * normalDotLight, 0.0))/ max(normalDotLight, 0.1);
    // Obtain the resolution of the current face of the cubemap
    float faceResolution = float(textureSize(pointShadowAtlas, 0).x) / 3.0;
    // Estimate the size of a texel in world space for the current face of the cubemap
    float worldTexelEstimate = 2.0 * lightDistance / faceResolution;
    // Regularize the bias based on the filter
    const float filterFootprint = 2.5;
    // Compute final bias for shadow mapping
    float bias = max(0.04 + 0.08 * (1.0 - normalDotLight),filterFootprint *worldTexelEstimate * slope);

    // Apply the computed bias to the world position to obtain the biased position for shadow mapping
    vec3 biasedPosition = worldPosition + toLight * min(bias, lightDistance * 0.5);
    // Transform the biased position into the light's clip space for shadow comparison
    vec4 biasedClip = gubo.pointShadowVP[lightIndex * 6 + face] * vec4(biasedPosition, 1.0);
    // Compute the depth of the biased position in the light's clip space for shadow comparison
    float receiverDepth = biasedClip.z / biasedClip.w;

    // Determine the size and origin of the current face within the shadow atlas
    ivec2 faceSize = textureSize(pointShadowAtlas, 0) / ivec2(3, 2 * POINT_SHADOW_LIGHTS);
    // Compute the UV coordinates within the current face of the shadow atlas
    vec2 faceUV = ndc.xy * 0.5 + 0.5;
    // Convert the UV coordinates to pixel coordinates within the face
    ivec2 pixel = clamp(
        ivec2(floor(faceUV * vec2(faceSize))),
        ivec2(0), faceSize - ivec2(1));
    // Compute the origin of the current face within the shadow atlas
    ivec2 tileOrigin = ivec2(face % 3, lightIndex * 2 + face / 3) * faceSize;
    // Initialize the visibility accumulator for percentage-closer filtering (PCF)
    float visibility = 0.0;
    // Perform a 3x3 PCF sampling around the current pixel to compute shadow visibility
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            // Compute the coordinates of the sample pixel within the current face of the shadow atlas
            ivec2 samplePixel = clamp(pixel + ivec2(offsetX, offsetY), ivec2(0), faceSize - ivec2(1));
            // Fetch the stored depth from the shadow atlas at the sample pixel
            float storedDepth = texelFetch(pointShadowAtlas, tileOrigin + samplePixel, 0).r;
            // Accumulate the visibility based on the comparison between the receiver depth and the stored depth
            visibility += receiverDepth <= storedDepth ? 1.0 : 0.0;
        }
    }
    // Return the average visibility as the final shadow factor
    return visibility / 9.0;
}

void main() {
    // Calculate the fragment normal vector in world space
    vec3 N = normalize(fragNormal);
    // Fetch the albedo color from the texture
    vec3 albedo = texture(albedoMap, fragUV).rgb;

    //--------------- Directional light calculations ---------------
    // Compute view direction vector (from fragment position to camera position)
    vec3 V = normalize(gubo.eyePos - fragPos);

    // Extract roughness and metallic values from material parameters
    float roughness = clamp(matParams.w, 0.05, 1.0);
    float metallic = clamp(matParams.z, 0.0, 1.0);

    // Base reflectance F0 (0.04 for dielectrics, albedo for metals)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Directional Light (Sun / Moon)
    vec3 L = normalize(-gubo.lightDir);
    // Computed once and reused for both the direct light and the sky ambient:
    // the same occluders that block the sun also occlude the sky dome.
    float sunVisibility = directionalVisibility(fragPos, N);
    // Compute the contribution of the directional light using the Cook-Torrance BRDF
    vec3 Lo = computeCookTorrance(N, V, L, gubo.lightColor.rgb, albedo, roughness, metallic, F0) * matParams.x;
    // Apply the visibility factor to account for shadows from the directional light
    Lo *= sunVisibility;
    

    //--------------- Point light calculations ---------------
    vec3 bounce = vec3(0.0); // Accumulator for the fake indirect bounce (torch light reflected by the walls)
    // Loop over all point lights and accumulate their contribution
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if (gubo.pointLightColor[i].a <= 0.01) 
            continue;
        // Vector from the fragment to the point light
        vec3 toLight = gubo.pointLightPos[i].xyz - fragPos;
        float distance = length(toLight);
        // Normalize the light direction vector for the point light
        vec3 Lp = toLight / max(distance, 0.0001);
        // Compute the attenuation based on distance and light radius
        float g = gubo.pointLightPos[i].w;
        float attenuation = min(pow(g / max(distance, 0.001), 2.0), 1.0);
        attenuation *= clamp(1.0 - pow(distance / (3.0 * g), 4.0), 0.0, 1.0);
        // Skip the light if it has no effect on the fragment
        if (attenuation <= 0.0 || dot(N, Lp) <= 0.0)
            continue;
        // Compute the radiance from the point light
        vec3 radianceP = gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation);
        // Accumulate the contribution of this point light for the fake indirect bounce
        bounce += gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation); // Accumulate (pre-shadow) for the fake indirect bounce fill
        if(i < POINT_SHADOW_LIGHTS)
            radianceP *= pointVisibility(fragPos, N, i);
        // Accumulate the contribution of this point light using the Cook-Torrance BRDF
        Lo += computeCookTorrance(N, V, Lp, radianceP, albedo, roughness, metallic, F0);
    }

    //---------- Compute the ambient component of the lighting ------------
    // Hemisphere ambient lighting based on surface orientation (N.y)
    const float indoorAmbientStrength = 0.5;
    float hemi = 0.5 + 0.5 * N.y; // 1.0 = facing the sky, 0.0 = facing the ground
    // Compute the ambient contribution from the sky and ground based on the hemisphere factor
    const vec3 skyTint = vec3(0.70, 0.80, 1.00); // cool bluish sky light
    const vec3 groundTint = vec3(0.55, 0.45, 0.35); // warm bounce from the ground
    // Indoor ambient colors for the ceiling and floor based on the hemisphere factor
    const vec3 indoorUpColor = vec3(0.065, 0.062, 0.055); // cool-ish stone bounce
    const vec3 indoorDownColor = vec3(0.032, 0.027, 0.022); // dark warm crevice color

    // Compute the sky occlusion factor based on sun visibility
    float skyOcclusion = mix(0.35, 1.0, sunVisibility);
    vec3 indoorAmbient = mix(indoorDownColor, indoorUpColor, hemi) * albedo;
    vec3 skyAmbient = (indoorAmbientStrength + 0.015 * max(gubo.lightColor.r, gubo.lightColor.b))
        * mix(groundTint, skyTint, hemi) * albedo * skyOcclusion;
    // Mix the indoor and sky ambient contributions based on the material parameter (x = 1.0 for outdoor)
    vec3 ambient = mix(indoorAmbient, skyAmbient, matParams.x); // x = 1.0 outdoor (same convention as BlinnFromPos)
    
    // Emissive contribution based on material parameter y
    vec3 emissive = matParams.y * albedo * vec3(2.0, 1.2, 0.5);
    // Bounce fill contribution to simulate indirect lighting
    vec3 bounceFill = bounce * albedo * 0.07;
    // Combine all lighting contributions to get the final color
    vec3 color = ambient + Lo + emissive + bounceFill;

    // Dynamic Fog
    if (gubo.fogColor.a > 0.0001) {
        float distToEye = length(gubo.eyePos - fragPos);
        float fogFactor = clamp(exp(-gubo.fogColor.a * distToEye), 0.0, 1.0);
        color = mix(gubo.fogColor.rgb, color, fogFactor);
    }

    // Reinhard Tone Mapping
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}