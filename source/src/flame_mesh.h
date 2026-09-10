#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "vertex.h"

// Creation in procedural way the flame mesh and load it into the model
inline void createFlameMesh(BaseProject *project, VertexDescriptor &vertexDescriptor, Model &flameModel)
{
    // Local vector of vertices for the flame mesh
    std::vector<Vertex> flameVertices; // Tmp vector for mesh vertices
    // Clear any existing vertices in the flame mesh for security
    flameModel.indices.clear();

    // ------- Add Triangle Function -------
    // Function to add a triangle to the flame mesh: takes three vertices as input and adds them to the vertex buffer and index buffer
    auto addTriangle = [&](glm::vec3 first, glm::vec3 second, glm::vec3 third)
    {
        // Calculate the normal perpendicular to the surface using the cross product of two edges of the triangle
        glm::vec3 normal = glm::normalize(glm::cross(second - first, third - first));
        // Loop through each of the three vertices of the triangle and add them to the vertex buffer and index buffer
        for (const glm::vec3 &position : {first, second, third})
        {
            // Add the index of the new vertex to the flame model's index buffer
            flameModel.indices.push_back(static_cast<uint32_t>(flameVertices.size()));
            // Add the new vertex to the flame mesh's vertex buffer
            flameVertices.push_back({position, glm::vec2(0.0f, position.y / 0.6f), normal}); // position and UV coordinates, normal vector
        }
    };

    // ---------- Add Tongue Function ----------
    // ----- Generate the geometry of a single tongue of fire with a curved conical/cylindrical shape
    // -- Parameters:
    // - base: the base position of the tongue
    // - radius: the radius of the tongue at its base
    // - height: the height of the tongue
    // - bend: the bending vector controlling the curvature of the tongue

    // The tongue is built using 3 rings of esagonal sections (hexagonal rings) + a tip vertex at the top
    auto addTongue = [&](glm::vec3 base, float radius, float height, glm::vec2 bend)
    {
        // Generate 3 rings along the height of the tongue, each of ring is composed by 6 sides
        constexpr int sides = 6;
        glm::vec3 rings[3][sides];
        // Initialize the rings array to store the positions of the vertices forming each ring
        const float levels[3] = {0.0f, 0.3f, 0.65f};  // Heights of the three rings: 0%, 30%, 65% of the total height
        const float widths[3] = {0.45f, 1.0f, 0.55f}; // Scale the radius of the three rings at each level

        // For each level, calculate the positions of the vertices forming the ring
        for (int level = 0; level < 3; ++level)
        {
            // Calculate the fraction of the height for the current level
            float fraction = levels[level];
            // Calculate the center position of the current ring based on the base position, bending vector, and height fraction
            // The use of a square shape means that the curvature is almost zero near the base
            // and increases more and more rapidly as it rises, creating a natural bending effect
            glm::vec3 center = base + glm::vec3(bend.x * fraction * fraction, height * fraction, bend.y * fraction * fraction);
            for (int side = 0; side < sides; ++side)
            {
                // Calculate the angle for the current side of the ring
                float angle = 6.2831853f * float(side) / float(sides);
                // Distribute 6 vertices evenly around the center of the ring in a circular pattern
                rings[level][side] = center + radius * widths[level] * glm::vec3(std::cos(angle), 0.0f, std::sin(angle));
            }
        }

        // Calculate the position of the tip of the tongue: max height and complete bending
        glm::vec3 tip = base + glm::vec3(bend.x, height, bend.y);
        // For each of 6 sides of the hexagonal ring
        for (int side = 0; side < sides; ++side)
        {
            // Calculate the index of the next vertex in the ring (wrapping around to the first vertex if necessary)
            int next = (side + 1) % sides;
            // Base: first ring
            addTriangle(base, rings[0][side], rings[0][next]);
            for (int level = 0; level < 2; ++level)
            {
                // Middle rings: connect the current ring to the next ring
                addTriangle(rings[level][side], rings[level + 1][side], rings[level][next]);
                addTriangle(rings[level][next], rings[level + 1][side], rings[level + 1][next]);
            }
            // Tip: connect the last ring to the tip vertex
            addTriangle(rings[2][side], tip, rings[2][next]);
        }
    };

    // ---------- Add Tongues of Fire ----------
    // Create multiple tongues of fire with varying positions, sizes, and bending directions
    addTongue(glm::vec3(0.0f), 0.12f, 0.60f, {0.04f, 0.01f});
    addTongue({-0.07f, 0.02f, 0.0f}, 0.075f, 0.43f, {-0.06f, 0.02f});
    addTongue({0.07f, 0.01f, 0.02f}, 0.07f, 0.35f, {0.06f, -0.03f});

    //------------- Copy the generated flame vertices to the flame model and initialize the mesh -------------
    // Resize the flame model's vertex buffer to accommodate all generated vertices
    flameModel.vertices.resize(flameVertices.size() * sizeof(Vertex));
    // Copy the generated flame vertices into the flame model's vertex buffer
    std::memcpy(flameModel.vertices.data(), flameVertices.data(), flameModel.vertices.size());
    // Initialize the flame model's mesh with the copied vertex data
    flameModel.initMesh(project, &vertexDescriptor);
}