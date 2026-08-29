// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>

#include <json.hpp>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"

// Uniform buffer object for the local parameters (per object)
struct UniformBufferObject {
	alignas(16) glm::mat4 mvpMat; // Matrix model view-projection
	alignas(16) glm::mat4 mMat; // Matrix model (local transformation of the object in the world)
};

// Uniform buffer object for the global parameters (per scene)
struct GlobalUniformBufferObject {
	alignas(16) glm::vec3 lightDir; // Direction of the light 
	alignas(16) glm::vec4 lightColor; // Light color and intensity
	alignas(16) glm::vec3 eyePos; // Position of the camera
};

// Vertex structure for the scene
struct Vertex {
	glm::vec3 pos; // Position of the vertex in 3D space
	glm::vec2 UV; // Texture coordinates for the vertex
};

// Camera structure to manage camera position and orientation
struct Camera {
		// Initial position
		glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 3.0f); 
		// Orientation angles
		float yaw = -glm::pi<float>() / 2.0f; // to control Horizontal rotation: initialized to -90 degrees
		float pitch = 0.0f; // Vertical rotation

		// Vectors 
		glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f); // Direction the camera is facing
		glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f); // Up direction for the camera
		glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f); // Right direction for the camera

		// Parameters to control the camera movement and sensitivity
		float moveSpeed   = 5.0f; // Speed of camera movement
		float mouseSensitivity = 0.002f; // Sensitivity of mouse movement for camera rotation
	};


class Skeleton26ReplaceName : public BaseProject {
	protected:
	// Here you list all the Vulkan objects you need:
	
	// Descriptor Layouts [define the structure of data that will be passed to the shaders]
	DescriptorSetLayout DSLlocal, DSLglobal;

	// Vertex formants, Pipelines [Shader couples] and Render passes
	VertexDescriptor VD; // Vertex format for the scene
	RenderPass RP; // Render pass for the scene
	Pipeline P; // Pipeline for the scene

	// Models, textures and Descriptors (values assigned to the uniforms)
	DescriptorSet DSglobal; // Descriptor set for global parameters

	// To support loading assets from a scene.json file
	Scene SC;
	std::vector<VertexDescriptorRef>  VDRs; // References to vertex descriptors
	std::vector<TechniqueRef> PRs; // References to techniques (pipelines)

	// to provide textual feedback
	TextMaker txt; // Object for displaying text on the screen
	
	// Other application parameters
	float Ar;	// Aspect ratio of the window (width/height)

	// Matrices for the camera
	glm::mat4 ViewPrj; // Combined view and projection matrix
	glm::mat4 View; // View matrix for the camera

	// Camera FPS instance
	Camera cam;
	
	// ----------- WINDOW CONFIGURATION AND CALLBACKS ----------------
	// Initializes the window parameters (size, title, resizable)
	void setWindowParameters() {
		// window size, title and initial background
		windowWidth = 800; // Initial window width (in pixels)
		windowHeight = 600; // Initial window height (in pixels)
		windowTitle = "Skeleton: place the name of your app here"; // Window title
    	windowResizable = GLFW_TRUE; // Allow the window to be resizable
		
		// Initial aspect ratio
		Ar = 4.0f / 3.0f; 
	}
	
	// What to do when the window changes size
	void onWindowResize(int w, int h) {
		std::cout << "Window resized to: " << w << " x " << h << "\n"; // Print the new window size to the console
		Ar = (float)w / (float)h; // Update the aspect ratio based on the new window size
		// Update Render Pass
		RP.width = w; // Update the width of the render pass to match the new window width
		RP.height = h; // Update the height of the render pass to match the new window height
		
		// updates the textual output
		txt.resizeScreen(w, h); // Update the text rendering system to accommodate the new window size
	}
	
	// ------------------ INITIALIZATION OF RESOURCES -------------------
	// Here you load and setup all your Vulkan Models and Texutures.
	// Here you also create your Descriptor set layouts and load the shaders for the pipelines
	void localInit() {
		// Descriptor Layouts [what will be passed to the shaders]
		// Initializes the local descriptor set layout (per object)
		DSLlocal.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObject), 1},
					{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
				  });
		// Initializes the global descriptor layout (for the global parameters)
		DSLglobal.init(this, {
					// this array contains the binding:
					// first  element : the binding number
					// second element : the type of element (buffer or texture)
					// third  element : the pipeline stage where it will be used
					{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(GlobalUniformBufferObject), 1}
				  });
		// Initialize the vertex descriptor (format of the vertices)
		VD.init(this, {
				  {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}
				}, {
				  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos),
				         sizeof(glm::vec3), POSITION},
				  {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV),
				         sizeof(glm::vec2), UV}
				});

		// Initializes the render passes
		RP.init(this);
		// Sets the initial background color for the render pass: blue sky
		RP.properties[0].clearValue = {0.0f,0.9f,1.0f,1.0f};

		// Pipelines [Shader couples]
		// The last array, is a vector of pointer to the layouts of the sets that will
		// be used in this pipeline. The first element will be set 0, and so on..
		// Initializes the pipeline with the vertex and fragment shaders, and the descriptor set layouts
		P.init(this, &VD, "shaders/toChangeSimplePos.vert.spv",
						  "shaders/toChangeBlinnFromPos.frag.spv",
						  {&DSLglobal, &DSLlocal});


		// Sets the size of the Descriptor Set Pool to allocate sufficient GPU space (it MUST be done before loading the scene)
		DPSZs.uniformBlocksInPool = 2;
		DPSZs.texturesInPool = 1;
		DPSZs.setsInPool = 2;

		// Configure the structures for automatic scene management
		VDRs.resize(1);
		VDRs[0].init("VDposUV",  &VD);

		PRs.resize(1); // This is the technique that will be used for the scene. It is a Blinn-Phong shader that uses the position and UV coordinates of the vertices.
		PRs[0].init("BlinnPos", {
							{&P, {//Pipeline and DSL for the main pass
							 /*DSLglobal*/{},
							 /*DSLlocal*/{
									/*t0*/{true,  0, {}}
								  }
								 }
								}
						  }, /*TotalNtextures*/1, &VD);

		// Loads the scene from a JSON file, which contains the models, textures, and their configurations
		if(SC.init(this, 1, VDRs, PRs, "assets/scenes/scene.json") != 0) {
			std::cout << "ERROR LOADING THE SCENE\n";
			exit(0);
		}

		// Initializes the textual output
		txt.init(this, windowWidth, windowHeight);

		// Submits the main command buffer
		submitCommandBuffer("main", 0, populateCommandBufferAccess, this); 

		// Configure the initial layout for the FPS on-screen printout
		txt.print(1.0f, 1.0f, "FPS:",1,"CO",false,false,true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});

	}
	
	// ------------------ PIPELINES AND DESCRIPTOR SETS MANAGEMENT -------------------
	// Here you create your pipelines and Descriptor Sets!
	// Effective creation of pipelines and resources allocation on GPU
	void pipelinesAndDescriptorSetsInit() {
		// Creates the render passes
		RP.create();
		
		// This creates a new pipeline (with the current surface), using its shaders for the provided render pass
		P.create(&RP);
		
		// Initializes the global descriptor set 
		DSglobal.init(this, &DSLglobal, {});
		
		// Here you define the data set
		// If the scene has textures coming from a render pass, the corresponding element of the technique must be
		// updated before calling SC.pipelinesAndDescriptorSetsInit();

		SC.pipelinesAndDescriptorSetsInit(); // Initialize the pipeline and descriptor sets for the scene, which includes creating descriptor sets for each model and texture in the scene.
		txt.pipelinesAndDescriptorSetsInit(); // Initialize the pipeline and descriptor sets for the text rendering system, which includes creating descriptor sets for the text textures.
	}

	// Here you destroy your pipelines and Descriptor Sets!
	void pipelinesAndDescriptorSetsCleanup() {
		P.cleanup();

		RP.cleanup();
		
		DSglobal.cleanup();
		
		SC.pipelinesAndDescriptorSetsCleanup();
		txt.pipelinesAndDescriptorSetsCleanup();
	}

	// Final cleanup of all resources before the application is closed
	// Here you destroy all the Models, Texture and Desc. Set Layouts you created!
	// You also have to destroy the pipelines
	void localCleanup() {
		DSLlocal.cleanup();
		DSLglobal.cleanup();

		P.destroy();

		RP.destroy();

		SC.localCleanup();
		txt.localCleanup();
	}
	
	// ------------------ COMMAND BUFFER MANAGEMENT -------------------
	// Registration of rendering commands to be sent to the GPU. This is where you define what will be drawn and how.
	// Here it is the creation of the command buffer:
	// You send to the GPU all the objects you want to draw,
	// with their buffers and textures

	// 
	static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
		// Simple trick to avoid having always 'T->'
		// in che code that populates the command buffer!
		Skeleton26ReplaceName *T = (Skeleton26ReplaceName *)Params;
		T->populateCommandBuffer(commandBuffer, currentImage);
	}

	// Function that populates the command buffer with the rendering commands for the current frame.
	void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
		
		// Offscreen pass - always required
		// begin standard pass
		RP.begin(commandBuffer, currentImage); // Begin the render pass for the current frame

		SC.populateCommandBuffer(commandBuffer, 0, currentImage); // Populate the command buffer with the rendering commands for the scene, using the first render pass (index 0)

		RP.end(commandBuffer); // End the render pass for the current frame
	}

	// ------------------ UNIFORM BUFFER MANAGEMENT -------------------
	// Recalcolate matrices and update data in the GPU memory at each frame
	// Here is where you update the uniforms. 
	// Very likely this will be where you will be writing the logic of your application.
	void updateUniformBuffer(uint32_t currentImage) {
		static bool debounce = false; 
		static int curDebounce = 0;

		// Handle the ESC button to exit the app
		if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		// Calculate the game logic and return the delta time since the last frame. This is used to update the scene and camera movement.
		float deltaT = GameLogic();
		
		// Calculate the light rotation 
		static float lightRotationAngle = 0.0f; // Static variable to keep track of rotation
		lightRotationAngle += -0.5f * deltaT; // Increment rotation angle based on delta time

		// Calculate the rotation matrix for the light direction based on the rotation angle
		const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(lightRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f)) * 
									glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::vec3 lightDir =  glm::vec3(lightView * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

		// Populate the data structure of global uniforms (light and camera))
		GlobalUniformBufferObject gubo{};
		gubo.lightDir = lightDir; // Update the light direction based on the rotation
		gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)*5.0f; // Update the light color and intensity
		gubo.eyePos = cam.cameraPos; // Update the eye position based on camera movement

		// Map the global uniform buffer object to the GPU memory for the current frame
		DSglobal.map(currentImage, &gubo, 0);

		// Defines the local parameters for the uniforms (for each 3D object in the scene)
		UniformBufferObject ubo{};		

		int instanceId;
		// Iterate through all instances in the scene and update their model matrices and MVP matrices based on the current view-projection matrix.
		for(instanceId = 0; instanceId < SC.TI[0].InstanceCount; instanceId++) {
			ubo.mMat = SC.TI[0].I[instanceId].Wm; // Get the world matrix for the current instance
			ubo.mvpMat = ViewPrj * ubo.mMat; // Calculate the model-view-projection matrix for the current instance
			
			// Map the local uniform buffer object to the GPU memory for the current frame and instance. 
			// The first descriptor set (DS[0]) is used for global parameters, and the second descriptor set (DS[1]) is used for local parameters specific to each instance.
			// DS[1] = Pchar pass (main render): set0=DSLglobal, set1=DSLlocal
			SC.TI[0].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0: Global UBO (light/camera)
			SC.TI[0].I[instanceId].DS[0][1]->map(currentImage, &ubo, 0); // Set 1: Local UBO (camera MVPs)
		}
		
		// Calculates and updates on the screen the FPS (Frame Per Second)
		static float elapsedT = 0.0f; // Accumulated time since the last FPS update
		static int countedFrames = 0; // Number of frames counted since the last FPS update
		
		countedFrames++;
		elapsedT += deltaT;
		if(elapsedT > 1.0f) { // Update the FPS display every second
			float Fps = (float)countedFrames / elapsedT; // Calculate the FPS based on the number of frames and elapsed time
			
			// Prepare the string to display the FPS on the screen
			std::ostringstream oss;
			oss << "FPS: " << Fps << "\n";
			txt.print(1.0f, 1.0f, oss.str(), 1, "CO", false, false, true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});
			
			// Reset the elapsed time and the frame count for the next FPS update
			elapsedT = 0.0f; 
		    countedFrames = 0; 
		}
		
		// Update the command buffer for the text 
		txt.updateCommandBuffer();
	}

	// ------------------ CAMERA AND INPUT MANAGEMENT -------------------
	// Update the camera orientation based on mouse movement
	void updateMouseInput(Camera& cam, float deltaX, float deltaY){
		cam.yaw += deltaX * cam.mouseSensitivity; // Update the yaw based on horizontal mouse movement
		cam.pitch -= deltaY * cam.mouseSensitivity; // Update the pitch based on vertical mouse movement (inverted to match typical FPS controls)

		// Clamp the pitch to avoid gimbal lock effect
		const float maxPitch = glm::radians(89.0f);
		cam.pitch = glm::clamp(cam.pitch, -maxPitch, maxPitch);

		// Calculate the new front vector using spherical coordinates 
		glm::vec3 direction; 
		direction.x = cos(cam.pitch) * cos(cam.yaw);
		direction.y = sin(cam.pitch);
		direction.z = cos(cam.pitch) * sin(cam.yaw);

		// Normalize the camera vectors
		cam.front = glm::normalize(direction); 
		cam.right = glm::normalize(glm::cross(cam.front, glm::vec3(0.0f, 1.0f, 0.0f))); // Assuming Y-up world
		cam.up = glm::normalize(glm::cross(cam.right, cam.front));
	}

	// Update the camera position based on keyboard input (WASD keys)
	void updateKeyboardInput(Camera& cam, float deltaT, GLFWwindow* window){
		float movementSpeed = cam.moveSpeed * deltaT; // Calculate movement speed based on delta time to ensure consistent movement regardless of frame rate

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			cam.cameraPos += cam.front * movementSpeed; // Move forward
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			cam.cameraPos -= cam.front * movementSpeed; // Move backward
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			cam.cameraPos -= cam.right * movementSpeed; // Move left
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			cam.cameraPos += cam.right * movementSpeed; // Move right
		}

	}
	
	// ------------------ GAME LOGIC -------------------
	float GameLogic() {
		// Camera FOV-y, Near Plane and Far Plane
		const float FOVy = glm::radians(45.0f); // Field of view in the y direction (in radians)
		const float nearPlane = 0.1f; // 
		const float farPlane = 100.f;

		// Retrieve the system input and the current frame time delta to update the camera position and orientation
		float deltaT; // Time elapsed since the last frame (in seconds)
		glm::vec3 m = glm::vec3(0.0f), r = glm::vec3(0.0f); // m = motion input, r = rotation input
		bool fire = false; // fire = action input 
		getSixAxis(deltaT, m, r, fire); // Retrieve input from a six-axis controller

		// Update camera orientation based on mouse movement
		// r.y = horizontal (yaw) input, r.x = vertical (pitch) input
		updateMouseInput(cam, r.y, r.x);

		// Update camera position based on keyboard input
		updateKeyboardInput(cam, deltaT, window);

		// --------- Projection matrix calculation ---------
		glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
		Prj[1][1] *= -1; // Invert Y axis for Vulkan (Y toward down in Vulkan)

		// --------- View matrix calculation (Look-in-direction) ---------
		View = glm::lookAt(cam.cameraPos, cam.cameraPos + cam.front, cam.up);
		
		// View-Projection
		ViewPrj = Prj * View;

		// Return the delta time for use in other parts of the application (for updating animations or physics)
		return deltaT;
	}
};


// ------------------------------ MAIN FUNCTION -------------------
// This is the main: probably you do not need to touch this!
// It creates the application object and runs it, handling any exceptions that may occur.
int main() {
    Skeleton26ReplaceName app; // Create an instance of the application class

    try {
        app.run(false); // Run the application, passing 'false' to indicate that ray tracing is not included
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl; // Print the error message to the standard error output
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS; // End the program successfully
}