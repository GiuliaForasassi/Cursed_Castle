#version 450 // GLSL version for Vulkan (4.5)

//--------------- Inputs: attributes of the fragment (from vertex shader) ---------------
layout(location = 0) in float flameHeight;
layout(location = 1) in float flameIntensity;

//--------------- Output: final color of the fragment ---------------
layout(location = 0) out vec4 outColor;

void main() {
    // Define the base colors for the flame (orange and yellow)
    // Note: The colors are defined in a high dynamic range (HDR) format (then tone-mapped later)
    vec3 orange = vec3(3.0, 0.35, 0.015);
    vec3 yellow = vec3(3.0, 2.2, 0.15);
    float blend = smoothstep(0.1, 0.85, flameHeight);

    // Linearly interpolate between orange and yellow based on blend, then apply the flame intensity
    vec3 color = mix(orange, yellow, blend) * flameIntensity;
    // Apply tone mapping to convert HDR color to LDR for display
    color = color / (color + vec3(1.0));
    // Set the final color of the fragment
    outColor = vec4(color, 1.0); 
}