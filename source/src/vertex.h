#pragma once

#include <glm/glm.hpp>

// Vertex structure for vertex of the 3D model
struct Vertex {
	glm::vec3 pos; // Position 3D (x, y, z)
	glm::vec2 UV; // Texture coordinates (u, v)
	glm::vec3 normal; // Normal vector for the vertex
};