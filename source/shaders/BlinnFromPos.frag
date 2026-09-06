//---------- SHADER FOR ROCKS, WALLS, WOOD AND GROUND -------

#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_POINT_LIGHTS 8

// Fragment shader for Blinn-Phong lighting model with support for directional and point lights

//--------------- Input and output definitions ---------------
// Input attributes: fragment (=pixel) position and texture coordinates
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;

// Output: final fragment color
layout(location = 0) out vec4 outColor;

// Texture 2D for Descriptor Set 1 
layout(binding = 1, set = 1) uniform sampler2D albedoMap;

// Buffer for global uniform data (light directions, colors, eye position, and point lights)  -> Descriptor Set 0
layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
    vec3 lightDir;
    vec4 lightColor;
    vec3 eyePos;

    // Point light vectors (position and color)
    vec4 pointLightPos[MAX_POINT_LIGHTS];
    vec4 pointLightColor[MAX_POINT_LIGHTS];
    // Fog color for atmospheric effects: RGB + a = density (0.0 = no fog, 1.0 = full fog)
    vec4 fogColor;
} gubo;

const float PI = 3.14159265359;

void main() {
    //--------------- Compute normal from fragment position ---------------
    // 1. Compute the partial derivatives of the fragment position
	vec3 X = dFdx(fragPos);
    vec3 Y = dFdy(fragPos);
    // 2. Compute the normal vector to the surface using the cross product of the partial derivatives
    vec3 N = -normalize(cross(X,Y));

    //--------------- Fetch albedo from texture ---------------
    // 2. Fetch the albedo color from the texture and apply gamma correction
    vec3 albedo = pow(texture(albedoMap, fragUV).rgb, vec3(2.2)); 

    //--------------- Directional light calculations ---------------
    // 3. Compute the directional light contribution
    // Compute view direction vector (from fragment position to camera position)
    vec3 V = normalize(gubo.eyePos - fragPos);

    // Compute the light direction vector
    vec3 L = normalize(-gubo.lightDir);

    // Compute the half-vector between the view and light directions
    vec3 H = normalize(V + L);
    vec3 radianceDir = gubo.lightColor.rgb;

    // Compute the dot products for the diffuse and specular components
    float NdotL = max(dot(N, L), 0.0);
    float HdotN = max(dot(H, N), 0.0);
    vec3 Lo = (albedo * NdotL + vec3(pow(HdotN,  128.0))* 0.04) * radianceDir;

    //--------------- Point light calculations ---------------
    // 4. Point lights (Torches)
    for(int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if(gubo.pointLightColor[i].a <= 0.01) 
            continue; // Light is inactive, skip

        vec3 toLight = gubo.pointLightPos[i].xyz - fragPos;
        float distance = length(toLight);
        vec3 Lp = toLight / max(distance, 0.0001);
        vec3 Hp = normalize(V + Lp);

        float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.05 * distance * distance);
        vec3 radiancePoint = gubo.pointLightColor[i].rgb * (gubo.pointLightColor[i].a * attenuation);

        float NdotLp = max(dot(N, Lp), 0.0);
        float HdotNp = max(dot(Hp, N), 0.0);
        // TODO: questo valore (150.0) può essere cambiato: provare altri valori
        vec3 LoPoint = (albedo * NdotLp + vec3(pow(HdotNp, 64.0)) *0.03) * radiancePoint;
        Lo += LoPoint;
    }
        
    //---------- Compute the ambient component of the lighting ------------
    // 5. Apply a small ambient term (0.015) to simulate indirect lighting
	vec3 ambient =(0.025 + 0.015 * max(gubo.lightColor.r, gubo.lightColor.b)) * albedo;
    vec3 color = ambient  + Lo;

    // 6. Apply exponential fog (enabled if fogColor.a > 0)
    if (gubo.fogColor.a > 0.0001) {
        float distToEye = length(gubo.eyePos - fragPos);
        float fogFactor = clamp(exp(-gubo.fogColor.a * distToEye), 0.0, 1.0);
        color = mix(gubo.fogColor.rgb, color, fogFactor);
    }

    // 7. Reinhard tone mapping to compress the dynamic range of the color
    color = color / (color + vec3(1.0));
    // 8. Apply gamma correction to convert the color from linear space to sRGB space
    color = pow(color, vec3(1.0 / 2.2));

    // Output the final color of the fragment
    outColor = vec4(color, 1.0);
}
