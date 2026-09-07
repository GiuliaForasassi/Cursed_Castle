// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>

#include <json.hpp>

#include "modules/Starter.hpp"
#ifndef TEXTMAKER_HPP_GUARD
#define TEXTMAKER_HPP_GUARD
#include "modules/TextMaker.hpp"
#endif
#ifndef SCENE_HPP_GUARD
#define SCENE_HPP_GUARD
#include "modules/Scene.hpp"
#endif
#include "camera.h"
#include "interactions.h"
#include "utils.h"
#include "game_state.h"


#include <limits>
#include <cstring>

#define MAX_POINT_LIGHTS 8

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

	// Point lights
	alignas(16) glm::vec4 pointLightPos[MAX_POINT_LIGHTS]; // Position of the point light: x, y, z, w (w can be used for padding or other purposes)
	alignas(16) glm::vec4 pointLightColor[MAX_POINT_LIGHTS]; // Color and intensity of the point light: r, g, b, intensity (and a = 1 if is light on, otherwise 0)
	alignas(16) glm::vec4 fogColor; // Color of the fog (r, g, b, a); a = density = no fog if 0
};

// Vertex structure for vertex of the 3D model
struct Vertex {
	glm::vec3 pos; // Position 3D (x, y, z)
	glm::vec2 UV; // Texture coordinates (u, v)
};

struct SkyBoxUniformBlock {
	// Matrix model view-projection for the skybox
	alignas(16) glm::mat4 mvpMat;  
	// 0.0 = Night, 1.0 = Day     
	alignas(16) float dayFactor;        
};

class Skeleton26ReplaceName : public BaseProject {
	protected:
	// Here you list all the Vulkan objects you need
	
	// Descriptor Layouts [define the structure of data that will be passed to the shaders]
	DescriptorSetLayout DSLlocal, DSLglobal;

	// Vertex formats, Pipelines [Shader couples] and Render passes
	VertexDescriptor VD; // Vertex format for the scene
	RenderPass RP; // Render pass for the scene
	Pipeline P; // Pipeline for the scene --> Blinn-Phong lighting model
	Pipeline P_CookTorrance;

	// Models, textures and Descriptors (values assigned to the uniforms)
	DescriptorSet DSglobal; // Descriptor set for global parameters

	// To support loading assets from a scene.json file
	Scene scene;
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

	// Interaction manager instance
	InteractionManager interactionManager;

	// Game state instance
	GameManager gameManager;
	
	// Current window size
	int currentWindowWidth = 800; // Current window width
	int currentWindowHeight = 600; // Current window height

	//------ Skybox objects ------------
	DescriptorSetLayout DSLsky;
    DescriptorSet DSsky;
    Pipeline P_SkyBox;
    Model M_SkyBox;
	Texture T_Sky;

    float currentDayFactor = 0.0f; // 0.0f = Notte, 1.0f = Giorno

	// ----------- WINDOW CONFIGURATION AND CALLBACKS ----------------
	// Initializes the window parameters (size, title, resizable)
	void setWindowParameters() {
		// window size, title and initial background
		windowWidth = 800; // Initial window width (in pixels)
		windowHeight = 600; // Initial window height (in pixels)
		windowTitle = "Cursed Castle"; // Window title
    	windowResizable = GLFW_TRUE; // Allow the window to be resizable
		
		// Initial aspect ratio
		Ar = 4.0f / 3.0f; 
	}
	
	// What to do when the window changes size
    void onWindowResize(int w, int h) {
        if (w <= 0 || h <= 0) return; // Avoid division by zero if the window is minimized

        std::cout << "Window resized to: " << w << " x " << h << "\n";
        Ar = (float)w / (float)h; // Update aspect ratio

		// Update the render pass dimensions to match the new window size
		RP.width = w;
        RP.height = h;
        
        // Update the text rendering system for the new resolution
        txt.resizeScreen(w, h);

        currentWindowWidth = w;
        currentWindowHeight = h;
    }
	
	// ------------------ INITIALIZATION OF RESOURCES -------------------
	// Here you load and setup all your Vulkan Models and Textures.
	// Here you also create your Descriptor set layouts and load the shaders for the pipelines
	void localInit() {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
		
		// Descriptor layout per lo SkyBox: UBO (b0), TexNotte (b1), TexGiorno (b2)
        // Descriptor layout per lo SkyBox: UBO (b0) e 1 Texture (b1)
        DSLsky.init(this, {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(SkyBoxUniformBlock), 1},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
        });

		// Initializes the render passes
		RP.init(this);
		// Sets the initial background color for the render pass: blue sky
		RP.properties[0].clearValue = {0.0f,0.9f,1.0f,1.0f};

		// Pipelines [Shader couples]
		// The last array, is a vector of pointer to the layouts of the sets that will
		// be used in this pipeline. The first element will be set 0, and so on..
		// Initializes the pipeline with the vertex and fragment shaders, and the descriptor set layouts

		//----------- Pipelines Initialization -----------
		// Pipeline 0: Blinn-Phong
        P.init(this, &VD, "shaders/SimplePos.vert.spv",
                          "shaders/BlinnFromPos.frag.spv",
                          {&DSLglobal, &DSLlocal});

        // Pipeline 1: Cook-Torrance (PBR)
        P_CookTorrance.init(this, &VD, "shaders/SimplePos.vert.spv",
                                       "shaders/CookTorranceFromPos.frag.spv",
                                       {&DSLglobal, &DSLlocal});

		// Pipeline 2: SkyBox
        P_SkyBox.init(this, &VD, "shaders/SkyBox.vert.spv", "shaders/SkyBox.frag.spv", {&DSLsky});
        P_SkyBox.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        P_SkyBox.polyModel = VK_POLYGON_MODE_FILL;
        P_SkyBox.CM = VK_CULL_MODE_NONE;

		// Carica il modello glTF e la texture singola dello Skybox
        M_SkyBox.init(this, &VD, "assets/models/skybox.gltf", GLTF);
        T_Sky.init(this, "assets/textures/Skybox_Puresky.png");

		// Sets the size of the Descriptor Set Pool to allocate sufficient GPU space (it MUST be done before loading the scene)
		DPSZs.uniformBlocksInPool = 10; // 1 for the global parameters, 1 for each object (in this case, we have 2 objects)
		DPSZs.texturesInPool = 10; // 1 for each object (in this case, we have 2 objects)
		DPSZs.setsInPool = 10; // 1 for the global parameters, 1 for each object (in this case, we have 2 objects)

		// Configure the structures for automatic scene management
		VDRs.resize(1);
		VDRs[0].init("VDposUV",  &VD);

		PRs.resize(2); 
		// This is the technique that will be used for the scene. It is a Blinn-Phong shader that uses the position and UV coordinates of the vertices.
		// Tecnica 0 -> BlinnPos
		PRs[0].init("BlinnPos", {
							{&P, {//Pipeline and DSL for the main pass
							 /*DSLglobal*/{},
							 /*DSLlocal*/{
									/*t0*/{true,  0, {}}
								  }
								 }
								}
						  }, /*TotalNtextures*/1, &VD);
		// Tecnica 1 -> CookTorrancePos
        PRs[1].init("CookTorrancePos", {
                            {&P_CookTorrance, { {}, { {true, 0, {}} } } }
                          }, 1, &VD);

		// Loads the scene from a JSON file, which contains the models, textures, and their configurations
		if(scene.init(this, 1, VDRs, PRs, "assets/scenes/scene.json") != 0) {
			std::cout << "ERROR LOADING THE SCENE\n";
			exit(0);
		}

		// Setup the interactions for the interactable objects in the scene
		setupInteractions();


		// Initializes the textual output
		txt.init(this, windowWidth, windowHeight);

		// Submits the main command buffer
		submitCommandBuffer("main", 0, populateCommandBufferAccess, this); 

		// Configure the initial layout for the FPS on-screen printout
		//txt.print(1.0f, 1.0f, "FPS:",1,"CO",false,false,true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});

	}
	
	// ------------------ PIPELINES AND DESCRIPTOR SETS MANAGEMENT -------------------
	// Here you create your pipelines and Descriptor Sets!
	// Effective creation of pipelines and resources allocation on GPU
	void pipelinesAndDescriptorSetsInit() {
		// Creates the render passes
		RP.create();
		
		// This creates a new pipeline (with the current surface), using its shaders for the provided render pass
		P.create(&RP);
		P_CookTorrance.create(&RP);
		P_SkyBox.create(&RP);
		
		// Initializes the global descriptor set 
		DSglobal.init(this, &DSLglobal, {});

		
		DSsky.init(this, &DSLsky, {
            T_Sky.getViewAndSampler()
        });
		
		// Here you define the data set
		// If the scene has textures coming from a render pass, the corresponding element of the technique must be
		// updated before calling SC.pipelinesAndDescriptorSetsInit();

		scene.pipelinesAndDescriptorSetsInit(); // Initialize the pipeline and descriptor sets for the scene, which includes creating descriptor sets for each model and texture in the scene.
		txt.pipelinesAndDescriptorSetsInit(); // Initialize the pipeline and descriptor sets for the text rendering system, which includes creating descriptor sets for the text textures.
	}

	// Here you destroy your pipelines and Descriptor Sets!
	void pipelinesAndDescriptorSetsCleanup() {
		P.cleanup();
		P_CookTorrance.cleanup();
		P_SkyBox.cleanup();

		RP.cleanup();
		
		DSglobal.cleanup();
		DSsky.cleanup();
		
		scene.pipelinesAndDescriptorSetsCleanup();
		txt.pipelinesAndDescriptorSetsCleanup();
	}

	// Final cleanup of all resources before the application is closed
	// Here you destroy all the Models, Texture and Desc. Set Layouts you created!
	// You also have to destroy the pipelines
	void localCleanup() {
		DSLlocal.cleanup();
		DSLglobal.cleanup();
		DSLsky.cleanup();

		P.destroy();
		P_CookTorrance.destroy();
		P_SkyBox.destroy();

		M_SkyBox.cleanup();
		T_Sky.cleanup();

		RP.destroy();

		scene.localCleanup();
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

		// Disegna lo SkyBox
        P_SkyBox.bind(commandBuffer);
        M_SkyBox.bind(commandBuffer);
        DSsky.bind(commandBuffer, P_SkyBox, 0, currentImage);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(M_SkyBox.indices.size()), 1, 0, 0, 0);


		scene.populateCommandBuffer(commandBuffer, 0, currentImage); // Populate the command buffer with the rendering commands for the scene, using the first render pass (index 0)

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

		// 1. Calcolo transizione graduale Notte -> Giorno (da 0.0 a 1.0 in ~3 secondi)
        if (gameManager.curseBroken) {
            currentDayFactor = glm::clamp(currentDayFactor + 0.35f * deltaT, 0.0f, 1.0f);
        } else {
            currentDayFactor = 0.0f;
        }
		
		// 2. Calculate the light rotation 
		static float lightRotationAngle = 0.0f; // Static variable to keep track of rotation
		lightRotationAngle += -0.5f * deltaT; // Increment rotation angle based on delta time

		// Calculate the rotation matrix for the light direction based on the rotation angle
		const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(lightRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f)) * 
									glm::rotate(glm::mat4(1), glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::vec3 lightDir =  glm::vec3(lightView * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

		//--------- Populate the data structure of global uniforms (light and camera)) ---------
		// 3. Popola GUBO (Luce direzionale sfumata con mix tra Notte e Giorno)
		GlobalUniformBufferObject gubo{};
		gubo.lightDir = lightDir; // Update the light direction based on the rotation
		gubo.eyePos = cam.getCameraPosition(); // Update the eye position based on camera movement

		// Colori luce: Notte (bluastra) vs Giorno (calda dorata)
        glm::vec4 nightLight = glm::vec4(0.2f, 0.3f, 0.6f, 1.0f) * 2.5f;
        glm::vec4 dayLight   = glm::vec4(1.0f, 0.95f, 0.85f, 1.0f) * 5.0f;
        gubo.lightColor = glm::mix(nightLight, dayLight, currentDayFactor);
		gubo.fogColor = glm::vec4(0.0f);

		// Menu screens: dimmed lighting + dark haze so the gold text stands out
		if (gameManager.currentState != GameState::PLAYING) {
			gubo.lightColor = glm::vec4(glm::vec3(gubo.lightColor) * 0.25f, 1.0f);
			gubo.fogColor = glm::vec4(0.02f, 0.02f, 0.05f, 0.06f);
		}

		//--------- Populate the point light data in the global uniform buffer ---------
		gubo.pointLightPos[0] = glm::vec4(0.0f, 3.0f, 20.0f, 0.0f);
		gubo.pointLightColor[0] = glm::vec4(3.0f, 1.8f, 0.9f, 1.0f);  // Active point light
		for(int i = 1; i < 4; i++) {
			gubo.pointLightPos[i] = glm::vec4(0.0f);
			gubo.pointLightColor[i] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Inactive point light (a = 0)
		}
		// Map the global uniform buffer object to the GPU memory for the current frame
		DSglobal.map(currentImage, &gubo, 0);

		// 4. Aggiorna Uniform Buffer per lo SkyBox (Matrice View senza traslazione + dayFactor)
        const float FOVy = glm::radians(45.0f);
		const float nearPlane = 0.1f;
        const float farPlane = 400.f;
        glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
        Prj[1][1] *= -1;

        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(View)); // Solo rotazione, niente traslazione
        SkyBoxUniformBlock skyUbo{};
        skyUbo.mvpMat = Prj * viewNoTranslation;
        skyUbo.dayFactor = currentDayFactor;
        DSsky.map(currentImage, &skyUbo, 0);

		//----------------------- Uniform BUffers --------------------
		// Defines the local parameters for the uniforms (for each 3D object in the scene)
		UniformBufferObject ubo{};		

		// Metodo più pulito e diretto: itera su tutte le istanze della scena (scene.I)
        for (int i = 0; i < scene.InstanceCount; i++) {
            ubo.mMat = scene.I[i]->Wm;
            ubo.mvpMat = ViewPrj * ubo.mMat;

            // Set 0: Global UBO (luci, camera, nebbia)
            // Set 1: Local UBO (matrice MVP e Model)
            scene.I[i]->DS[0][0]->map(currentImage, &gubo, 0);
            scene.I[i]->DS[0][1]->map(currentImage, &ubo, 0);
        }
		
		// 6. Aggiorna FPS e Text
		// Calculates and updates on the screen the FPS (Frame Per Second)
		static float elapsedT = 0.0f; // Accumulated time since the last FPS update
		static int countedFrames = 0; // Number of frames counted since the last FPS update
		
		countedFrames++;
		elapsedT += deltaT;
		if(elapsedT > 1.0f) { // Update the FPS display every second
			float Fps = (float)countedFrames / elapsedT; // Calculate the FPS based on the number of frames and elapsed time

			// Prepare the string to display the FPS on the screen
			if(gameManager.currentState == GameState::PLAYING) {
				std::ostringstream oss;
				oss << "FPS: " << Fps << "\n";
				txt.print(1.0f, 1.0f, oss.str(), 1, "CO", false, false, true, TAL_RIGHT, TRH_RIGHT, TRV_BOTTOM, {1.0f,0.0f,0.0f,1.0f}, {0.8f,0.8f,0.0f,1.0f});
			} else if(txt.Blocks.count(1)) {
				txt.removeText(1);
			}

			// Reset the elapsed time and the frame count for the next FPS update
			elapsedT = 0.0f;
    		countedFrames = 0;
		}
		
		// Update the command buffer for the text 
		txt.updateCommandBuffer();
	}

	// ----------------- Setup Interactions Objects ------------------
	// Setup the interactions for the scene by adding info and trigger interactions for specific objects
	// TODO: You can add your own interactions here
	void setupInteractions() {
		// Register relics interactions
		interactionManager.addRelicInteraction("Book", "Press E to pick up Sacred Grimoire");
        interactionManager.addRelicInteraction("Cup", "Press E to pick up Holy Chalice");
        interactionManager.addRelicInteraction("Sword", "Press E to pick up Cursed Blade");
		interactionManager.addAltarInteraction("Altar");
		interactionManager.addDoorInteraction("Door_main");
		// + 0.1719 is the local Z-coordinate of the hinge relative to the door model's origin
		interactionManager.setDoorHinge("Door_main", +0.1719f, +1.0f);
		interactionManager.addDoorInteraction("Door_L");
		interactionManager.addLockedDoorInteraction("Door_locked", "Key", "Press E to unlock Door");
	}

	// ------------------ GAME LOGIC -------------------
	float GameLogic() {
		// Camera FOV-y, Near Plane and Far Plane
		const float FOVy = glm::radians(45.0f); // Field of view in the y direction (in radians)
		const float nearPlane = 0.1f; // 
		const float farPlane = 400.f; // 

		// Retrieve the system input and the current frame time delta to update the camera position and orientation
		float deltaT; // Time elapsed since the last frame (in seconds)
		glm::vec3 m = glm::vec3(0.0f), r = glm::vec3(0.0f); // m = motion input, r = rotation input
		bool fire = false; // fire = action input 
		getSixAxis(deltaT, m, r, fire); // Retrieve input from a six-axis controller

		//------------------- 1. Handle Game State Input and UI -------------------
		gameManager.handleInput(window, txt, currentWindowWidth, currentWindowHeight);
    	gameManager.updateUI(txt, currentWindowWidth, currentWindowHeight);

		// ------------------ 2. Process mouse/keyboard and Gameplay ------------------
		if (gameManager.currentState == GameState::PLAYING) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hide and capture the cursor during gameplay

			// Process mouse input to update the camera's orientation based on the current mouse position
			cam.processMouseInput(window);
			cam.processKeyboardInput(window, deltaT, scene);

			interactionManager.updateAnimations(scene, deltaT, cam.getCameraPosition());


			// --------- 3. Check for interactions with objects in the scene ---------
			int nearestInteractableObjIndex = interactionManager.findNearestInteractable(scene, cam, 4.0f); // Find the nearest interactable object
			if (nearestInteractableObjIndex >= 0 && interactionManager.infoTextTimer <= 0.0f){
				std::string promptText = interactionManager.getPrompt(nearestInteractableObjIndex, gameManager);
				txt.print(0.0f, 0.75f, promptText, 2, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f,0.0f,0.0f,0.0f}, getTextScale(currentWindowWidth, currentWindowHeight), getTextScale(currentWindowWidth, currentWindowHeight)); // Display the prompt for the nearest interactable object
			} else{
				txt.removeText(2); // Remove any previous prompts 
			}

			// Handle the 'E' key interaction
			bool ePressedNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
			interactionManager.handleEKey(ePressedNow, nearestInteractableObjIndex, scene, cam, gameManager); 

			// TODO: rivedi come viene gestito il wrapping del testo informativo in base alla larghezza disponibile
			if(interactionManager.infoTextTimer > 0.0f){
				interactionManager.infoTextTimer -= deltaT; // Decrease the timer for displaying info text
				if(interactionManager.infoTextTimer > 0.0f){
					int actualWidth, actualHeight;
					glfwGetFramebufferSize(window, &actualWidth, &actualHeight);
					float scale = getTextScale(actualWidth, actualHeight);
					float availablePixels = actualWidth * 0.85f;
					float maxWidthUnscaled = availablePixels / scale;
					int maxCharsPerLine = estimateMaxCharsPerLine(actualWidth, scale);
					// std::string wrappedInfoText = wrapText(interactionManager.currentInfoText, maxCharsPerLine);
					std::string wrappedInfoText = wrapTextToWidth(txt, interactionManager.currentInfoText, 4, maxWidthUnscaled);
					txt.print(0.0f, 0.40f, wrappedInfoText, 3, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f,0.0f,0.0f,0.0f}, scale, scale);
				}
				else{
					txt.removeText(3); // Remove the info text when the timer expires
				}
			}

			// Check victory condition (e.g., all relics collected and brought to the altar)
			if (gameManager.curseBroken) {
                glm::vec3 playerPos = cam.getCameraPosition();
                // Esempio: se il giocatore esce dal portone oltre Z = 25.0f (adatterai la coordinata in base alla nuova mappa)
                if (playerPos.z > 25.0f) {
                    gameManager.clearMenuTexts(txt);
                    txt.removeText(2); // Rimuove eventuali prompt di interazione
                    txt.removeText(3); // Rimuove eventuali testi informativi
                    gameManager.currentState = GameState::VICTORY;
                }
            }
		} else {
			// If we are in the menu (not interacting with objects), show the cursor and remove any interaction texts
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			txt.removeText(2);
			txt.removeText(3);
		}

		// --------- Projection matrix calculation ---------
		glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
		Prj[1][1] *= -1; // Invert Y axis for Vulkan (Y toward down in Vulkan)

		// --------- View matrix calculation (Look-in-direction) ---------
		View = glm::lookAt(cam.getCameraPosition(), cam.getCameraPosition() + cam.getCameraFront(), cam.getCameraUp());
		
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