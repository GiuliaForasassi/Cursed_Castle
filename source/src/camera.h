
// Camera structure to manage camera position and orientation
class Camera {
	// Initial position
	glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 3.0f); 
	// Orientation angles
	float yaw = -glm::pi<float>() / 2.0f; // to control Horizontal rotation: initialized to -90 degrees
	float pitch = 0.0f; // Vertical rotation

	// Vectors 
	glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f); // Direction the camera is facing
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // Up direction for the camera
	glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f); // Right direction for the camera

	// Parameters to control the camera movement and sensitivity
	float moveSpeed   = 5.0f; // Speed of camera movement
	float mouseSensitivity = 0.002f; // Sensitivity of mouse movement for camera rotation

	double lastMouseX = 0.0;
	double lastMouseY = 0.0; // Last mouse positions for camera control
	bool firstMouse = true; // Flag to check if it's the first mouse movement

    // Update the camera's orientation based on mouse movement (deltaX and deltaY represent the change in mouse position)
    public:
        void updateOrientation(float deltaX, float deltaY) {
            // Update the yaw based on horizontal mouse movement
            yaw += deltaX * mouseSensitivity;
            // Update the pitch based on vertical mouse movement
            pitch -= deltaY * mouseSensitivity;

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
            updateOrientation(-(float)dx / 10.0f, -(float)dy / 10.0f);
        }

        // Update the camera position based on keyboard input (WASD keys) and collision detection with the scene
        void processKeyboardInput(GLFWwindow* window, float deltaT,
                                const std::function<bool(glm::vec3, float)>& collidesWithScene) {
            // Calculate movement speed based on delta time to ensure consistent movement regardless of frame rate
            float movementSpeed = moveSpeed * deltaT;
            // Radius of the player for collision detection
            const float playerRadius = 1.0f;

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
            if (!collidesWithScene(tryPosX, playerRadius)) 
                cameraPos.x = tryPosX.x;

            glm::vec3 tryPosZ = cameraPos + glm::vec3(0.0f, 0.0f, movement.z);
            if (!collidesWithScene(tryPosZ, playerRadius)) 
                cameraPos.z = tryPosZ.z;
        }

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