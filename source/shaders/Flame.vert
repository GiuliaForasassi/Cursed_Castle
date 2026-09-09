#version 450 // GLSL version for Vulkan (4.5)

//----- Input: vertex attributes -----
layout(location = 0) in vec3 inPosition; // Vertex position
layout(location = 1) in vec2 inUV; // Vertex UV coordinates

//----- Uniform block for flame animation -----
layout(set = 0, binding = 0) uniform FlameUniformBlock {
    mat4 mvpMat; // Model-View-Projection matrix for transforming vertex positions
    vec4 animation; // x: time, y: phase, z: unused, w: unused
} flame;

//----- Output: attributes to the fragment shader -----
layout(location = 0) out float flameHeight; // Height of the flame 
layout(location = 1) out float flameIntensity; // Intensity of the flame 

void main() {
    // Extract time and phase from the animation uniform block
    float time = flame.animation.x;
    float phase = flame.animation.y;
    // Calculate the height factor based on the vertex's UV coordinate
    float heightFactor = clamp(inUV.y, 0.0, 1.0);
    // Movement that increases quadratically with height
    float movementWeight = heightFactor * heightFactor;

    // Calculate the flicker effect based on time and phase
    float flicker = 0.85 + 0.15 * sin(time * 7.0 + phase);
    // Apply the calculated movement and flicker to the vertex position
    vec3 position = inPosition;
    position.x += 0.035 * movementWeight *
        sin(time * 3.5 + phase + heightFactor * 2.0);
    position.z += 0.025 * movementWeight *
        sin(time * 4.3 + phase + heightFactor * 3.0);
    position.y *= 0.95 + 0.05 * flicker;

    // Pass the calculated height and intensity to the fragment shader
    flameHeight = heightFactor;
    flameIntensity = flicker;
    gl_Position = flame.mvpMat * vec4(position, 1.0);
}