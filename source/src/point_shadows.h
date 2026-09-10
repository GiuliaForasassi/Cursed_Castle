#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Generates 6 view-projection matrices for a point light's shadow cubemap
inline std::array<glm::mat4, 6> makePointShadowMatrices(
    const glm::vec3 &lightPosition, // Position of the point light
    float nearPlane,                // Near clipping plane distance
    float farPlane                  // Far clipping plane distance
)
{
    // Six directions in which the virtual camera positioned on the light must face: +X, -X, +Y, -Y, +Z, -Z
    // These are the 6 faces of a cubemap
    const std::array<glm::vec3, 6> directions = {{{1.0f, 0.0f, 0.0f},
                                                  {-1.0f, 0.0f, 0.0f},
                                                  {0.0f, 1.0f, 0.0f},
                                                  {0.0f, -1.0f, 0.0f},
                                                  {0.0f, 0.0f, 1.0f},
                                                  {0.0f, 0.0f, -1.0f}}};

    // Up vectors corresponding to each of the 6 directions
    const std::array<glm::vec3, 6> upVectors = {{{0.0f, -1.0f, 0.0f},
                                                 {0.0f, -1.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {0.0f, 0.0f, -1.0f},
                                                 {0.0f, -1.0f, 0.0f},
                                                 {0.0f, -1.0f, 0.0f}}};

    // Create a perspective projection matrix for the point light's shadow cubemap faces
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
    projection[1][1] *= -1.0f;

    // Compute the view-projection matrices for each face of the cubemap
    std::array<glm::mat4, 6> matrices{};
    for (size_t face = 0; face < matrices.size(); ++face)
    {
        matrices[face] = projection * glm::lookAt(lightPosition, lightPosition + directions[face], upVectors[face]);
    }
    return matrices;
}