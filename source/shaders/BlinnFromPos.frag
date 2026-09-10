//---------- SHADER FOR ROCKS, WALLS, WOOD AND GROUND -------

#version 450 // GLSL version 4.5
#extension GL_ARB_separate_shader_objects : enable // Enable separate shader objects for modular shader programming

#define MAX_POINT_LIGHTS 22 // Number of point lights
#define POINT_SHADOW_LIGHTS 22 // Number of point light shadow-casting lights

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
layout(set = 0, binding = 2) uniform sampler2D pointShadowAtlas; // Shadow atlas for point light shadows

//UNIFORM SET 0: Global parameters (directional light, camera position, point lights, fog)
// ------- GUBO
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

const float PI = 3.14159265359;

// --------------- Directional Light Shadow Visibility ---------------
// Computes the visibility of a fragment with respect to the directional light, taking into account shadows. Returns 1.0 if fully visible, 0.0 if fully in shadow.
// It takes a point on the scene and its normal, projects it into the light space and returns the percentage direct light that reaches it (1.0 = fully visible, 0.0 = fully in shadow)
float directionalVisibility(vec3 worldPosition) {
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
    
    // Compute the normal vector
	vec3 N = normalize(fragNormal);
    // Fetch the albedo color from the texture
    vec3 albedo = texture(albedoMap, fragUV).rgb;

    //--------------- Directional light calculations ---------------
    // Compute view direction vector (from fragment position to camera position)
    vec3 V = normalize(gubo.eyePos - fragPos); // Direction from fragment to camera
    vec3 L = normalize(-gubo.lightDir); // Direction from fragment to directional light
    vec3 H = normalize(V + L); // Half-vector between view and light directions
    vec3 radianceDir = gubo.lightColor.rgb;

    // Compute the dot products for the diffuse and specular components
    float NdotL = max(dot(N, L), 0.0);
    float HdotN = max(dot(H, N), 0.0);
    // Computed once and reused for both the direct light and the sky ambient:
    // the same occluders that block the sun also occlude the sky dome.
    float sunVisibility = directionalVisibility(fragPos);
    // compute the contribution of the directional light to the fragment color 
    // Sum the contributions of the diffuse and specular components for the directional light
    vec3 Lo = (albedo + vec3(pow(HdotN, 128.0)) * 0.04) * NdotL * radianceDir * matParams.x;
    Lo *= sunVisibility;

    //--------------- Point light calculations ---------------
    // Accumulator for the fake indirect bounce (torch light reflected by the walls)
    vec3 bounce = vec3(0.0); 
    for(int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if(gubo.pointLightColor[i].a <= 0.01) // Alpha < 0.01: skip inactive point lights
            continue; 

        // Vector from fragment to point light
        vec3 toLight = gubo.pointLightPos[i].xyz - fragPos; 
        float distance = length(toLight);
        // Direction from fragment to point light
        vec3 Lp = toLight / max(distance, 0.0001); 
        // Half-vector
        vec3 Hp = normalize(V + Lp); 
        // 
        float g = gubo.pointLightPos[i].w; // Range of the point light
        // Quadratic attenuation based on distance and range of the point light
        float attenuation = min(pow(g / max(distance, 0.001), 2.0), 1.0); 
        // Multiply the attenuation by a roll-off factor
        attenuation *= clamp(1.0 - pow(distance / (3.0 * g), 4.0), 0.0, 1.0); 
        if (attenuation <= 0.0 || dot(N, Lp) <= 0.0)
            continue;
        // Radiance of the point light after attenuation
        vec3 radiancePoint = gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation); 
        // Accumulate (pre-shadow) for the fake indirect bounce fill
        bounce += gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation);
        if(i < POINT_SHADOW_LIGHTS)
            radiancePoint *= pointVisibility(fragPos, N, i);

        // Scalar product of normal and light direction for diffuse component
        float NdotLp = max(dot(N, Lp), 0.0); 
        // Scalar product of half-vector and normal for specular component
        float HdotNp = max(dot(Hp, N), 0.0); 
        // Total contribution of the point light to the fragment color
        vec3 LoPoint = (albedo + vec3(pow(HdotNp, 64.0)) * 0.03) * NdotLp * radiancePoint; 
        Lo += LoPoint;
    }
        
    //---------- Compute the ambient component of the lighting ------------
    // Hemisphere ambient lighting based on surface orientation (N.y)
    const float indoorAmbientStrength = 0.5;
    float hemi = 0.5 + 0.5 * N.y; // 1.0 = surface facing the sky, 0.0 = facing the ground

    // Outdoor: cool sky tint from above, warm dirt-bounce tint from below
    const vec3 skyTint = vec3(0.70, 0.80, 1.00); // cool bluish sky light
    const vec3 groundTint = vec3(0.55, 0.45, 0.35); // warm bounce from the ground

    // Indoor: dim ambient, slightly cooler on up-facing surfaces (light falling
    // from the ceiling area) and warmer on down-facing ones (bounce from the floor)
    const vec3 indoorUpColor = vec3(0.065, 0.062, 0.055); // cool-ish stone bounce
    const vec3 indoorDownColor = vec3(0.032, 0.027, 0.022); // dark warm crevice color
    // Compute sky occlusion and ambient contributions
    float skyOcclusion = mix(0.35, 1.0, sunVisibility);
    vec3 skyAmbient = (indoorAmbientStrength + 0.015 * max(gubo.lightColor.r, gubo.lightColor.b))
        * mix(groundTint, skyTint, hemi) * albedo * skyOcclusion;
    vec3 indoorAmbient = mix(indoorDownColor, indoorUpColor, hemi) * albedo;
    vec3 ambient = mix(indoorAmbient, skyAmbient, matParams.x);
    // Compute the emissive component of the lighting
    vec3 emissive = matParams.y * albedo * vec3(2.0, 1.2, 0.5);
    // Compute the bounce fill component of the lighting
    vec3 bounceFill = bounce * albedo * 0.07;
    // Combine all lighting components to get the final color before fog and tone mapping
    vec3 color = ambient + Lo + emissive + bounceFill;

    // Apply exponential fog (enabled if fogColor.a > 0)
    if (gubo.fogColor.a > 0.0001) {
        float distToEye = length(gubo.eyePos - fragPos);
        float fogFactor = clamp(exp(-gubo.fogColor.a * distToEye), 0.0, 1.0);
        color = mix(gubo.fogColor.rgb, color, fogFactor);
    }

    // Reinhard tone mapping to compress values of final color HDR that exceed the displayable range
    color = color / (color + vec3(1.0));

    // Output the final color of the fragment
    outColor = vec4(color, 1.0);
}
