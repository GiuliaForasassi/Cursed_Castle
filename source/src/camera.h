
#pragma once
#ifndef SCENE_HPP_GUARD
#define SCENE_HPP_GUARD
#include "modules/Scene.hpp"
#endif
// Camera structure to manage camera position and orientation

class Camera {
	// Initial position
	glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 44.0f); 
	// Orientation angles
	float yaw = -glm::pi<float>() / 2.0f; // to control Horizontal rotation: initialized to -90 degrees
	float pitch = 0.0f; // Vertical rotation

	// Vectors 
	glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f); // Direction the camera is facing
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // Up direction for the camera
	glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f); // Right direction for the camera

	// Parameters to control the camera movement and sensitivity
	float moveSpeed   = 5.0f; // Speed of camera movement
	float mouseSensitivity = 0.0012f; // Sensitivity of mouse movement for camera rotation

	double lastMouseX = 0.0;
	double lastMouseY = 0.0; // Last mouse positions for camera control
	bool firstMouse = true; // Flag to check if it's the first mouse movement

    // Update the camera's orientation based on mouse movement (deltaX and deltaY represent the change in mouse position)
    public:

        void updateOrientation(float deltaX, float deltaY) {
            // Update the yaw based on horizontal mouse movement
            yaw -= deltaX * mouseSensitivity;
            // Update the pitch based on vertical mouse movement
            pitch += deltaY * mouseSensitivity;

            // Clamp the pitch to avoid gimbal lock effect
            const float maxPitch = glm::radians(89.0f);
            pitch = glm::clamp(pitch, -maxPitch, maxPitch);

            // Calculate the new front direction vector based on the updated yaw and pitch angles (using spherical coordinates)
            glm::vec3 direction;
            direction.x = cos(pitch) * cos(yaw);
            direction.y = sin(pitch);
            direction.z = cos(pitch) * sin(yaw);

            // Update the camera's front, right, and up vectors based on the new direction
            front = glm::normalize(direction);
            right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
            up = glm::normalize(glm::cross(right, front));
        }

        // Process mouse input to update the camera's orientation based on the current mouse position
        void processMouseInput(GLFWwindow* window) {
            // Get the current mouse position from the GLFW window
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            // If this is the first mouse movement, initialize the last mouse positions to the current positions
            if (firstMouse) {
                lastMouseX = xPos;
                lastMouseY = yPos;
                firstMouse = false;
            }
            // Calculate the change in mouse position since the last frame
            double dx = xPos - lastMouseX;
            double dy = yPos - lastMouseY;

            // Update the last mouse positions to the current positions
            lastMouseX = xPos;
            lastMouseY = yPos;

            // Update the camera's orientation based on the mouse movement
            updateOrientation(-static_cast<float>(dx), -static_cast<float>(dy));
        }

        // Update the camera position based on keyboard input (WASD keys) and collision detection with the scene
        void processKeyboardInput(GLFWwindow* window, float deltaT, Scene& scene) {
            // Calculate movement speed based on delta time to ensure consistent movement regardless of frame rate
            float movementSpeed = moveSpeed * deltaT;
            // Radius of the player for collision detection
            const float playerRadius = 0.65f;

            // Initialize movement vector: it accumulates the movement direction based on key presses
            glm::vec3 movement(0.0f);
            // Update the movement vector based on keyboard input (WASD keys)
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement += front;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= front;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
            // Prevent vertical movement (Y-axis) to keep the player on the ground
            movement.y = 0.0f;
            // Normalize the movement vector and scale it by the movement speed if there is any movement
            if (glm::length(movement) > 0.0001f) {
                movement = glm::normalize(movement) * movementSpeed;
            }

            // Attempt to move the camera along the X and Z axes separately, checking for collisions with the scene
            glm::vec3 tryPosX = cameraPos + glm::vec3(movement.x, 0.0f, 0.0f);
            if (!collidesWithScene(tryPosX, playerRadius, scene))
                cameraPos.x = tryPosX.x;;

            glm::vec3 tryPosZ = cameraPos + glm::vec3(0.0f, 0.0f, movement.z);
            if (!collidesWithScene(tryPosZ, playerRadius, scene)) 
                cameraPos.z = tryPosZ.z;
        }

        // ------------------ Collision detection -------------------
        // Check if the player (vertical cylinder) collides with any object in
        // the scene. A cylinder (circle in XZ + height interval in Y) matches
        // low props (tables, chairs, barrels, chests): a sphere centered at
        // eye height (y=2) floats above them and never collides.
        // AABBextents from getExtents() are already in WORLD space (the 8
        // corners are transformed by the collider's world matrix).
        bool collidesWithScene(glm::vec3 pos, float radius, Scene& scene) {
            // Cylinder spans from the ground up to (a bit above) eye height
            const float cylBottom = pos.y - 1.7f;   // feet (y ≈ 0.3)
            const float cylTop    = pos.y + 0.1f;   // just above the head
            const glm::vec2 center(pos.x, pos.z);   // cylinder axis in XZ

            for (int i = 0; i < scene.InstanceCount; i++) { // Iterate through all instances in the scene
                Collider *c = scene.I[i]->C; // Get the collider for the current instance
                if (c == nullptr)
                    continue; // Skip if the instance does not have a collider

                // World-space AABB of this instance's collider
                AABBextents E = c->getExtents();

                // Overlap in Y?
                if (E.yMax < cylBottom || E.yMin > cylTop) continue;

                // Closest point of the box (in XZ) to the cylinder axis
                float cx = glm::clamp(center.x, E.xMin, E.xMax);
                float cz = glm::clamp(center.y, E.zMin, E.zMax);
                float dx = center.x - cx;
                float dz = center.y - cz;
                if (dx * dx + dz * dz <= radius * radius)
                    return true; // Cylinder intersects this collider
            }
            return false;
        }

        void resetCamera() {
            cameraPos = glm::vec3(0.0f, 2.0f, 44.0f); 
            yaw   = -glm::pi<float>() / 2.0f;   // guarda verso -Z, cioè verso il castello
            pitch = 0.0f;
            firstMouse = true;                  // evita lo scatto del mouse al respawn
            updateOrientation(0.0f, 0.0f);
        }

        void resetMouseTracking() {
            firstMouse = true;
        }

        // ------------------ Getters for camera parameters -------------------
        const glm::vec3& getCameraPosition() const {
            return cameraPos;
        }

        const glm::vec3& getCameraFront() const {
            return front;
        }

        const glm::vec3& getCameraRight() const {
            return right;
        }   

        const glm::vec3& getCameraUp() const {
            return up;
        }
};