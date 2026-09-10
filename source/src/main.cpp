// THIS IS THE FILE YOU MUST START FROM!

// This has been adapted from the Vulkan tutorial
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
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
#include "vertex.h"
#include "flame_mesh.h"
#include "point_shadows.h"

#define MAX_POINT_LIGHTS 20
#define POINT_SHADOW_LIGHTS 20

// UBO: Variables specific to each object (instance) in the scene (local parameters)
struct UniformBufferObject
{
	alignas(16) glm::mat4 mvpMat;	   // Matrix model view-projection
	alignas(16) glm::mat4 mMat;		   // Matrix model (local transformation of the object in the world)
	alignas(16) glm::vec4 lightParams; // Light parameters for the object: x = 1.0 outdoor (receives the directional light), 0.0 inside the castle
};

// GUBO: Variables equal for all objects in the scene (global parameters)
struct GlobalUniformBufferObject
{
	// Directional light parameters
	alignas(16) glm::vec3 lightDir;	  // Direction of the light
	alignas(16) glm::vec4 lightColor; // Light color and intensity

	// Camera parameters
	alignas(16) glm::vec3 eyePos; // Position of the camera

	// Point lights parameters
	alignas(16) glm::vec4 pointLightPos[MAX_POINT_LIGHTS];		  // Position of the point light: x, y, z, w (w can be used for padding or other purposes)
	alignas(16) glm::vec4 pointLightColor[MAX_POINT_LIGHTS];	  // Color and intensity of the point light: r, g, b, intensity (and a = 1 if is light on, otherwise 0)
	alignas(16) glm::vec4 fogColor;								  // Color of the fog (r, g, b, a); a = density = no fog if 0
	alignas(16) glm::mat4 lightVP;								  // Light view-projection matrix for shadow mapping
	alignas(16) glm::mat4 pointShadowVP[6 * POINT_SHADOW_LIGHTS]; // Transformation matrices for the six faces of the point light's shadow cube map
};

// Skybox uniform block containing the model view-projection matrix and the day-night factor
struct SkyBoxUniformBlock
{
	alignas(16) glm::mat4 mvpMat; // Matrix model view-projection for the skybox
	alignas(16) float dayFactor;  // 0.0 = Night, 1.0 = Day
};

// Flame uniform block containing the model view-projection matrix and animation parameters
struct FlameUniformBlock
{
	alignas(16) glm::mat4 mvpMat;	 // Matrix model view-projection for the flame
	alignas(16) glm::vec4 animation; // Animation parameters for the flame
};

class CursedCastle : public BaseProject
{
protected:
	// --------- Window and camera ------------------
	int currentWindowWidth = 800;  // Current window width
	int currentWindowHeight = 600; // Current window height
	float Ar;					   // Aspect ratio of the window (width/height)
	Camera cam;
	glm::mat4 ViewPrj; // Combined view and projection matrix
	glm::mat4 View;	   // View matrix for the camera

	// ------------- Game state and interface -----------------
	InteractionManager interactionManager;
	GameManager gameManager;
	TextMaker txt;
	float totalTime = 0.0f;			// Total elapsed time since the start of the application
	float victoryMenuTimer = -1.0f; // Timer for the victory menu, negative value indicates inside the castle

	// ------------ Scene and material parameters -----------------
	Scene scene;
	std::vector<VertexDescriptorRef> VDRs; // References to vertex descriptors
	std::vector<TechniqueRef> PRs;		   // References to techniques (pipelines)
	std::vector<glm::vec4> instanceParams; // x = outdoor, y = emissive

	// ------------- Main rendering: Blinn Phong and Cook-Torrance ---------
	VertexDescriptor VD; // Vertex format for the scene
	DescriptorSetLayout DSLlocal, DSLglobal;
	DescriptorSet DSglobal;	 // Descriptor set for global parameters
	RenderPass RP;			 // Render pass for the scene
	Pipeline P;				 // Pipeline for the scene --> Blinn-Phong lighting model
	Pipeline P_CookTorrance; // Pipeline for the scene --> Cook-Torrance lighting model

	// ------ Skybox ------------
	DescriptorSetLayout DSLsky;
	DescriptorSet DSsky;
	Pipeline P_SkyBox;
	Model M_SkyBox;
	Texture T_Sky;
	float currentDayFactor = 0.0f; // 0.0f = Notte, 1.0f = Giorno

	// -------------- Shared rendering resources --------------
	VertexDescriptor VDshadow; // Vertex format for the shadow mapping pass
	TextureSampler TS_Shadow;

	// --------------- Directional light and shadow -------------
	const glm::vec3 sunDirection = glm::normalize(glm::vec3(-1.0f, -2.0f, -1.0f)); // Direction of the main directional light (sun)
	glm::mat4 LightVP;															   // Light view-projection matrix for the main directional light's shadow mapping
	RenderPass RP_Shadow;														   // Render pass for shadow mapping
	Pipeline P_Shadow;															   // Pipeline for the main directional light's shadow mapping

	// ----------- Point lights and shadows ----------------
	static constexpr int PointShadowFaceSize = 512;							 // Size of each face of the point light's shadow cubemap
	std::vector<glm::vec3> torchPositions;									 // Positions of the point light sources (torches)
	std::array<glm::mat4, 6 * POINT_SHADOW_LIGHTS> PointLightShadowMatrices; // View-projection matrices for the point light's shadow cubemap faces
	RenderPass RP_PointShadow;												 // Render pass for the point light's shadow cubemap
	std::array<Pipeline, 6 * POINT_SHADOW_LIGHTS> P_PointShadowFaces;		 // Pipelines for each face of the point light's shadow cubemap
	std::vector<Collider> shadowModelBounds;								 // Bounding volumes for models used in shadow mapping --> optimization purposes

	// ----------- Flame ----------------
	VertexDescriptor VDflame; // Vertex format for the flame
	Model M_Flame;
	DescriptorSetLayout DSLflame;
	Pipeline P_Flame;
	std::vector<DescriptorSet> DSflames;	// Descriptor sets for each flame/torch instance
	std::vector<glm::mat4> flameTransforms; // Transformation matrices for each flame instance

	// ----------- WINDOW CONFIGURATION AND CALLBACKS ----------------
	// Initializes the window parameters (size, title, resizable)
	void setWindowParameters()
	{
		// window size, title and initial background
		windowWidth = 800;			   // Initial window width (in pixels)
		windowHeight = 600;			   // Initial window height (in pixels)
		windowTitle = "Cursed Castle"; // Window title
		windowResizable = GLFW_TRUE;   // Allow the window to be resizable

		// Initial aspect ratio
		Ar = 4.0f / 3.0f;
	}

	// What to do when the window changes size
	void onWindowResize(int w, int h)
	{
		if (w <= 0 || h <= 0)
			return; // Avoid division by zero if the window is minimized

		std::cout << "Window resized to: " << w << " x " << h << "\n";
		Ar = (float)w / (float)h; // Update aspect ratio
		currentWindowWidth = w;
		currentWindowHeight = h;
	}

	// ------------------ INITIALIZATION OF RESOURCES -------------------
	// Here you load and setup all your Vulkan Models and Textures.
	// Here you also create your Descriptor set layouts and load the shaders for the pipelines
	void localInit()
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		//-------- Initializes the vertex descriptor ---------
		VD.init(this, {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}},
				{{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos), sizeof(glm::vec3), POSITION}, {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV), sizeof(glm::vec2), UV}, {0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal), sizeof(glm::vec3), NORMAL}});

		VDshadow.init(this, {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}},
					  {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos), sizeof(glm::vec3), POSITION}});

		VDflame.init(this, {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}},
					 {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos), sizeof(glm::vec3), POSITION}, {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV), sizeof(glm::vec2), UV}});

		// Descriptor Layouts [what will be passed to the shaders]
		//------- General setup ---------
		DSLlocal.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObject), 1},
							 {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}});
		DSLglobal.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(GlobalUniformBufferObject), 1},
							  {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
							  {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1}}); // binding 2 for shadow map

		// Initializes the render passes
		RP.init(this);
		// Sets the initial background color for the render pass: blue sky
		RP.properties[0].clearValue = {0.0f, 0.9f, 1.0f, 1.0f};

		// Pipeline 0: Blinn-Phong
		P.init(this, &VD, "shaders/SimplePos.vert.spv",
			   "shaders/BlinnFromPos.frag.spv",
			   {&DSLglobal, &DSLlocal});

		// Pipeline 1: Cook-Torrance (PBR)
		P_CookTorrance.init(this, &VD, "shaders/SimplePos.vert.spv",
							"shaders/CookTorranceFromPos.frag.spv",
							{&DSLglobal, &DSLlocal});

		// ----------- Flame mesh creation ----------------
		createFlameMesh(this, VDflame, M_Flame);
		DSLflame.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, sizeof(FlameUniformBlock), 1}});

		// Pipeline for the flame
		P_Flame.init(this, &VDflame, "shaders/Flame.vert.spv", "shaders/Flame.frag.spv", {&DSLflame});
		P_Flame.CM = VK_CULL_MODE_NONE; // Disable back-face culling for the flame mesh

		// --------------- SkyBox setup ---------------
		// SkyBox descriptor layout: UBO (b0) and 1 texture (b1)
		DSLsky.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(SkyBoxUniformBlock), 1},
						   {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}});

		// Pipeline 2: SkyBox
		// Using the shadow vertex descriptor for the SkyBox pipeline becuase I need only the position attribute
		P_SkyBox.init(this, &VDshadow, "shaders/SkyBox.vert.spv", "shaders/SkyBox.frag.spv", {&DSLsky});
		P_SkyBox.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // Set the depth comparison operation for the SkyBox pipeline
		P_SkyBox.polyModel = VK_POLYGON_MODE_FILL;		  // Set the polygon mode for the SkyBox pipeline
		P_SkyBox.CM = VK_CULL_MODE_NONE;				  // Disable back-face culling for the SkyBox pipeline

		// Load the skybox glTF model and its single texture
		M_SkyBox.init(this, &VD, "assets/models/skybox.gltf", GLTF);
		T_Sky.init(this, "assets/textures/Skybox_Puresky.png");

		// ------------ Shadow Mapping ------------------------
		// 1. Creation of rander pass only depth (no color attachment)
		// 2. Define the syncronization barriers between the write (shadow pass) and the read (final pass)
		// 3. Create the shadow mapping pipeline and configure its properties
		// 4. Calculation of the light's view-projection matrix for shadow mapping

		// Get the standard attachment properties for a depth-only render pass
		auto shadowProperties = *RenderPass::getStandardAttchmentsProperties(AT_DEPTH_ONLY, this);
		// Choose between D32_SFLOAT and D16_UNORM the format that GPU can support both as a depth-stencil attachment and as a sampled image
		shadowProperties[0].format = findSupportedFormat(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

		// Define the pipeline stages for depth testing (early and late fragment tests)
		const VkPipelineStageFlags depthStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		std::vector<VkSubpassDependency> shadowDependencies = {
			// Avoid read/write hazards between the shadow pass and the final pass
			{VK_SUBPASS_EXTERNAL, 0,
			 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | depthStages,
			 depthStages,
			 VK_ACCESS_SHADER_READ_BIT |
				 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			 0},
			{0, VK_SUBPASS_EXTERNAL,
			 depthStages,
			 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			 VK_ACCESS_SHADER_READ_BIT,
			 0}};

		// Initialize the shadow map texture sampler
		TS_Shadow.init(
			this,
			VK_FILTER_NEAREST,					   // magnification filter
			VK_FILTER_NEAREST,					   // minification filter
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // U coordinate wrapping mode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // V coordinate wrapping mode
			VK_SAMPLER_MIPMAP_MODE_NEAREST,		   // mipmap filtering mode
			VK_FALSE,							   // anisotropic filtering disabled
			1.0f,								   // max anisotropy (not used since anisotropic filtering is disabled)
			0.0f								   // mipmap LOD bias
		);

		// Create render pass and pipeline for shadow mapping
		// Render pass offscreen at resolution 2048x2048, qith 1 sample using attachment and dependencies definied
		RP_Shadow.init(this, 4096, 4096, 1, &shadowProperties, &shadowDependencies, false);
		// Pipelines for shadow mapping
		P_Shadow.init(this, &VDshadow, "shaders/Shadow.vert.spv",
					  "shaders/Shadow.frag.spv",
					  {&DSLglobal, &DSLlocal},
					  {{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}});
		// Disable back-face culling for the shadow pass
		P_Shadow.CM = VK_CULL_MODE_NONE;

		const glm::vec3 lightTarget(3.0f, 0.0f, 5.0f);
		glm::mat4 lightProjection = glm::ortho(-60.0f, 60.0f, -60.0f, 60.0f, 1.0f, 400.0f);
		lightProjection[1][1] *= -1.0f;

		LightVP = lightProjection * glm::lookAt(lightTarget - sunDirection * 200.0f, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));

		// ------------------ Point Light Shadow Mapping ------------------
		RP_PointShadow.init(this, PointShadowFaceSize * 3, PointShadowFaceSize * 2 * POINT_SHADOW_LIGHTS, 1, &shadowProperties, &shadowDependencies, false);
		// Initialize the pipelines for each face of the point light's shadow cubemap
		for (size_t face = 0; face < P_PointShadowFaces.size(); ++face)
		{
			auto &pipeline = P_PointShadowFaces[face];
			pipeline.init(this, &VDshadow, "shaders/Shadow.vert.spv",
						  "shaders/Shadow.frag.spv",
						  {&DSLglobal, &DSLlocal},
						  {{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)}});
			// Disable back-face culling for the shadow pass
			pipeline.CM = VK_CULL_MODE_NONE;

			// Determine the position of each face in the atlas 2D
			const int offsetX = static_cast<int>(face % 3) * PointShadowFaceSize;
			const int offsetY = static_cast<int>(face / 3) * PointShadowFaceSize;
			// Define the transformation from the normalized space to pixel coordinates within the atlas
			pipeline.setViewport({{static_cast<float>(offsetX), static_cast<float>(offsetY), static_cast<float>(PointShadowFaceSize), static_cast<float>(PointShadowFaceSize), 0.0f, 1.0f}});
			// Specify the region of the atlas that this face will render to
			pipeline.setScissor({{{offsetX, offsetY}, {PointShadowFaceSize, PointShadowFaceSize}}});
		}

		// ------------------- Scene setup -------------------
		// Sets the size of the Descriptor Set Pool to allocate sufficient GPU space (it MUST be done before loading the scene)
		DPSZs.uniformBlocksInPool = 10; // 1 for the global parameters, 1 for each object (in this case, we have 2 objects)
		DPSZs.texturesInPool = 10;		// 1 for each object (in this case, we have 2 objects)
		DPSZs.setsInPool = 10;			// 1 for the global parameters, 1 for each object (in this case, we have 2 objects)

		// Configure the structures for automatic scene management
		VDRs.resize(1);
		VDRs[0].init("VDposUV", &VD);
		// Pipeline setup for the scene rendering
		PRs.resize(2);
		// This is the technique that will be used for the scene. It is a Blinn-Phong shader that uses the position and UV coordinates of the vertices.
		// Tecnica 0 -> BlinnPos
		PRs[0].init("BlinnPos", {{&P, {// Pipeline and DSL for the main pass
									   /*DSLglobal*/ {},
									   /*DSLlocal*/ {/*t0*/ {true, 0, {}}}}}},
					/*TotalNtextures*/ 1, &VD);
		// Tecnica 1 -> CookTorrancePos
		PRs[1].init("CookTorrancePos", {{&P_CookTorrance, {{}, {{true, 0, {}}}}}}, 1, &VD);

		// Loads the scene from a JSON file, which contains the models, textures, and their configurations
		if (scene.init(this, 1, VDRs, PRs, "assets/scenes/scene.json") != 0)
		{
			throw std::runtime_error("Error loading assets/scenes/scene.json");
		}

		// ------------------- Flat atlas texture setup -------------------
		// Configure the sampler for the flat atlas texture --> for the grass tile texture
		Texture *flatAtlas = scene.T[scene.TextureIds.at("Flat_Atlas")];
		flatAtlas->sampler->cleanup();
		flatAtlas->sampler->init(
			this,
			VK_FILTER_NEAREST,
			VK_FILTER_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_FALSE,
			1.0f,
			0.0f);
		// Classify instances as outdoor or indoor
		classifyInstances();
		// Setup the material properties (metallic and roughness) for each instance in the scene
		setupMaterials();

		// Compute the bounding volumes for each model in the scene to optimize shadow mapping
		shadowModelBounds.resize(scene.ModelCount);
		for (int modelIndex = 0; modelIndex < scene.ModelCount; ++modelIndex)
		{ // Fit the axis-aligned bounding box for the current model
			shadowModelBounds[modelIndex].fitAABB(scene.M[modelIndex]);
		}

		// ------------------- Torch lights setup -------------------
		// Collect the positions of the torch lights in the scene
		collectTorchLights();
		// Determine the number of active torch lights and resize the corresponding data structures accordingly
		const int flameCount = std::min(static_cast<int>(torchPositions.size()), MAX_POINT_LIGHTS);
		DSflames.resize(flameCount);
		DPSZs.uniformBlocksInPool += flameCount;
		DPSZs.setsInPool += flameCount;

		// Generate the view-projection matrices for the point light's shadow cubemap faces
		for (auto &matrix : PointLightShadowMatrices)
		{
			matrix = glm::mat4(1.0f);
		}
		const int shadowLightCount = std::min(static_cast<int>(torchPositions.size()), POINT_SHADOW_LIGHTS);
		for (int lightIndex = 0; lightIndex < shadowLightCount; ++lightIndex)
		{
			auto matrices = makePointShadowMatrices(torchPositions[lightIndex], 0.05f, 12.0f);
			for (size_t face = 0; face < matrices.size(); ++face)
			{
				PointLightShadowMatrices[lightIndex * 6 + face] = matrices[face];
			}
		}

		// ------------------- Interactions and final setup -------------------
		// Setup the interactions for the interactable objects in the scene
		setupInteractions();

		// Initializes the textual output
		txt.init(this, windowWidth, windowHeight);

		// Submits the main command buffer
		submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

		// Configure the initial layout for the FPS on-screen printout
		// txt.print(1.0f, 1.0f, "FPS:",1,"CO",false,false,true,TAL_RIGHT,TRH_RIGHT,TRV_BOTTOM,{1.0f,0.0f,0.0f,1.0f},{0.8f,0.8f,0.0f,1.0f});
	}

	// ------------------ PIPELINES AND DESCRIPTOR SETS MANAGEMENT -------------------
	// Here you create your pipelines and Descriptor Sets!
	// Effective creation of pipelines and resources allocation on GPU
	// After a resize the framework recreates the swapchain before calling this;
	// use its extent so render passes and text match the new framebuffers.
	void pipelinesAndDescriptorSetsInit()
	{
		RP.width = swapChainExtent.width;
		RP.height = swapChainExtent.height;
		txt.resizeScreen(swapChainExtent.width, swapChainExtent.height);

		RP.create();				 // Create the main render pass
		RP_Shadow.create();			 // Create the shadow render pass
		P_Shadow.create(&RP_Shadow); // Create the shadow pipeline

		RP_PointShadow.create(); // Create the point shadow render pass
		for (auto &pipeline : P_PointShadowFaces)
		{
			pipeline.create(&RP_PointShadow); // Create the point shadow pipeline
		}

		// This creates a new pipeline (with the current surface), using its shaders for the provided render pass
		P.create(&RP);
		P_CookTorrance.create(&RP);
		P_SkyBox.create(&RP);

		P_Flame.create(&RP);
		for (auto &descriptor : DSflames)
		{
			descriptor.init(this, &DSLflame, {});
		}

		// Create a descriptor image info for the shadow map, which will be used in the global descriptor set
		VkDescriptorImageInfo shadowInfo{
			TS_Shadow.getSampler(),
			RP_Shadow.attachments[0].getView(0),
			RP_Shadow.properties[0].finalLayout};

		// Create a descriptor image info for the point light's shadow map, which will be used in the global descriptor set
		VkDescriptorImageInfo pointShadowInfo{
			TS_Shadow.getSampler(),
			RP_PointShadow.attachments[0].getView(0),
			RP_PointShadow.properties[0].finalLayout};

		// Initializes the global descriptor set (with the shadow map information)
		DSglobal.init(this, &DSLglobal, {shadowInfo, pointShadowInfo});

		for (auto &technique : PRs)
		{
			technique.PT[0].texDefs[0] = {
				{false, 0, shadowInfo},
				{false, 0, pointShadowInfo}};
		}

		// Initialize the sky descriptor set with the sky texture
		DSsky.init(this, &DSLsky, {T_Sky.getViewAndSampler()});

		// Here you define the data set
		// If the scene has textures coming from a render pass, the corresponding element of the technique must be
		// updated before calling SC.pipelinesAndDescriptorSetsInit();

		scene.pipelinesAndDescriptorSetsInit(); // Initialize the pipeline and descriptor sets for the scene, which includes creating descriptor sets for each model and texture in the scene.
		txt.pipelinesAndDescriptorSetsInit();	// Initialize the pipeline and descriptor sets for the text rendering system, which includes creating descriptor sets for the text textures.
	}

	// Here you destroy your pipelines and Descriptor Sets!
	void pipelinesAndDescriptorSetsCleanup()
	{

		for (auto &pipeline : P_PointShadowFaces)
		{
			pipeline.cleanup();
		}
		RP_PointShadow.cleanup();

		for (auto &descriptor : DSflames)
		{
			descriptor.cleanup();
		}
		P_Flame.cleanup();
		P_Shadow.cleanup();
		RP_Shadow.cleanup();
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
	void localCleanup()
	{
		clearCommandBuffers();
		txt.M = nullptr;
		for (auto &pipeline : P_PointShadowFaces)
		{
			pipeline.destroy();
		}
		RP_PointShadow.destroy();
		P_Flame.destroy();
		DSLflame.cleanup();

		P_Shadow.destroy();
		RP_Shadow.destroy();
		TS_Shadow.cleanup();

		DSLlocal.cleanup();
		DSLglobal.cleanup();

		DSLsky.cleanup();

		P.destroy();
		P_CookTorrance.destroy();
		P_SkyBox.destroy();

		M_SkyBox.cleanup();
		M_Flame.cleanup();
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
	static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params)
	{
		// Simple trick to avoid having always 'T->'
		// in che code that populates the command buffer!
		CursedCastle *T = (CursedCastle *)Params;
		T->populateCommandBuffer(commandBuffer, currentImage);
	}

	// OPTIMIZATION FUNCTION: culling for each face for static object
	bool canCastPointShadow(Instance *instance, size_t faceIndex)
	{
		const size_t lightIndex = faceIndex / 6;
		const std::string &id = *instance->id;
		if (id.rfind("Door", 0) == 0 ||
			id == "Book" || id == "Cup" || id == "Sword" ||
			id == "Golden_Key" || id == "Key")
		{
			return true;
		}

		Collider bounds = shadowModelBounds[instance->Mid];
		bounds.setWorldMatrix(instance->Wm);
		const AABBextents extents = bounds.getExtents();

		const glm::vec3 minimum(extents.xMin, extents.yMin, extents.zMin);
		const glm::vec3 maximum(extents.xMax, extents.yMax, extents.zMax);
		const glm::vec3 lightPosition = torchPositions[lightIndex];
		const glm::vec3 closest = glm::clamp(lightPosition, minimum, maximum);
		const glm::vec3 difference = closest - lightPosition;
		constexpr float cullingRadius = 11.5f;

		if (glm::dot(difference, difference) > cullingRadius * cullingRadius)
		{
			return false;
		}

		const glm::mat4 transposedVP =
			glm::transpose(PointLightShadowMatrices[faceIndex]);

		const std::array<glm::vec4, 6> planes = {{transposedVP[3] + transposedVP[0],
												  transposedVP[3] - transposedVP[0],
												  transposedVP[3] + transposedVP[1],
												  transposedVP[3] - transposedVP[1],
												  transposedVP[2],
												  transposedVP[3] - transposedVP[2]}};

		const glm::vec3 center = (minimum + maximum) * 0.5f;
		const glm::vec3 halfExtents = (maximum - minimum) * 0.5f;
		constexpr float margin = 0.01f;

		for (const glm::vec4 &plane : planes)
		{
			const glm::vec3 normal(plane);
			const float signedDistance = glm::dot(normal, center) + plane.w;
			const float projectedRadius = glm::dot(glm::abs(normal), halfExtents);

			if (signedDistance + projectedRadius < -margin * glm::length(normal))
			{
				return false;
			}
		}

		return true;
	}

	// Function that populates the point shadow pass in the command buffer
	void populatePointShadowPass(VkCommandBuffer commandBuffer, int currentImage)
	{
		// Begin the point shadow render pass
		RP_PointShadow.begin(commandBuffer, 0);
		// Iterate over each point light face and render its shadow map
		if (!torchPositions.empty())
		{
			for (size_t face = 0; face < P_PointShadowFaces.size(); ++face)
			{
				if (face / 6 >= torchPositions.size())
				{
					continue;
				}
				// Binding the face with the pipeline
				auto &pipeline = P_PointShadowFaces[face];
				pipeline.bind(commandBuffer);

				// Send the view-projection matrix for that specific face as a constant push
				vkCmdPushConstants(commandBuffer, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &PointLightShadowMatrices[face]);
				for (int index = 0; index < scene.InstanceCount; ++index)
				{
					// Retrieve the instance and check if it can cast a point shadow for the current face
					Instance *instance = scene.I[index];
					if (!canCastPointShadow(instance, face))
						continue;
					Model *model = scene.M[instance->Mid];

					// For each istance: bind the local descriptor set
					instance->DS[0][1]->bind(commandBuffer, pipeline, 1, currentImage);
					model->bind(commandBuffer);

					// Issue the draw call for the current instance
					vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model->indices.size()), 1, 0, 0, 0);
				}
			}
		}
		RP_PointShadow.end(commandBuffer);
	}

	// Function that populates the command buffer with the rendering commands for the current frame.
	void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage)
	{
		// -------- SHADOW PASS --------
		// Register the shadow pass in the command buffer
		RP_Shadow.begin(commandBuffer, 0);
		// Bind the shadow pipeline
		P_Shadow.bind(commandBuffer);

		// Push the light view-projection matrix to the vertex shader as a push constant
		vkCmdPushConstants(
			commandBuffer,
			P_Shadow.pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(glm::mat4),
			&LightVP);

		// Render each instance of the scene for the shadow pass using the shadow pipeline and descriptor sets
		for (int index = 0; index < scene.InstanceCount; ++index)
		{
			// Retrieve the instance and model for the current index
			Instance *instance = scene.I[index];
			Model *model = scene.M[instance->Mid];
			// Bind the local descriptor set for the shadow pass
			instance->DS[0][1]->bind(
				commandBuffer, P_Shadow, 1, currentImage);
			// Bind the model and issue the draw call for the shadow pass
			model->bind(commandBuffer);
			vkCmdDrawIndexed(
				commandBuffer,
				static_cast<uint32_t>(model->indices.size()),
				1, 0, 0, 0);
		}
		// End of shadow pass
		RP_Shadow.end(commandBuffer);

		populatePointShadowPass(commandBuffer, currentImage);

		// Offscreen pass - always required
		// begin standard pass
		RP.begin(commandBuffer, currentImage); // Begin the render pass for the current frame

		scene.populateCommandBuffer(commandBuffer, 0, currentImage); // Populate the command buffer with the rendering commands for the scene, using the first render pass (index 0)

		// Render the flame instances
		P_Flame.bind(commandBuffer);
		M_Flame.bind(commandBuffer);
		for (auto &descriptor : DSflames)
		{
			descriptor.bind(commandBuffer, P_Flame, 0, currentImage);
			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(M_Flame.indices.size()), 1, 0, 0, 0);
		}

		// Disegna lo SkyBox
		P_SkyBox.bind(commandBuffer);
		M_SkyBox.bind(commandBuffer);
		DSsky.bind(commandBuffer, P_SkyBox, 0, currentImage);
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(M_SkyBox.indices.size()), 1, 0, 0, 0);
		RP.end(commandBuffer); // End the render pass for the current frame
	}

	// ------------------ UNIFORM BUFFER MANAGEMENT -------------------
	// Recalcolate matrices and update data in the GPU memory at each frame
	// Here is where you update the uniforms.
	// Very likely this will be where you will be writing the logic of your application.
	void updateUniformBuffer(uint32_t currentImage)
	{
		static bool debounce = false;
		static int curDebounce = 0;

		// Handle the ESC button to exit the app
		if (glfwGetKey(window, GLFW_KEY_ESCAPE))
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		// Calculate the game logic and return the delta time since the last frame. This is used to update the scene and camera movement.
		float deltaT = GameLogic();

		// 1. Calcolo transizione graduale Notte -> Giorno (da 0.0 a 1.0 in ~3 secondi)
		if (gameManager.curseBroken)
		{
			currentDayFactor = glm::clamp(currentDayFactor + 0.35f * deltaT, 0.0f, 1.0f);
		}
		else
		{
			currentDayFactor = 0.0f;
		}

		//--------- Populate the data structure of gubo (light and camera)) ---------
		// 2. Popola GUBO (Luce direzionale sfumata con mix tra Notte e Giorno)
		GlobalUniformBufferObject gubo{};
		gubo.lightDir = sunDirection; // Update the light direction based on the rotation
		gubo.lightVP = LightVP;		  // Update the light view-projection matrix for shadow mapping
		// Calculate the transformation matrices for the six faces of the point light's shadow cube map
		for (size_t face = 0; face < PointLightShadowMatrices.size(); ++face)
		{
			gubo.pointShadowVP[face] = PointLightShadowMatrices[face];
		}
		gubo.eyePos = cam.getCameraPosition(); // Update the eye position based on camera movement

		// Color light: Night (dark purple, curse) vs Day (warm golden)
		glm::vec4 nightLight = glm::vec4(0.35f, 0.15f, 0.55f, 1.0f) * 2.5f;
		glm::vec4 dayLight = glm::vec4(1.0f, 0.95f, 0.85f, 1.0f) * 5.0f;
		gubo.lightColor = glm::mix(nightLight, dayLight, currentDayFactor);
		gubo.fogColor = glm::vec4(0.0f);

		// Menu screens: dimmed lighting + dark haze so the gold text stands out
		if (gameManager.currentState != GameState::PLAYING)
		{
			gubo.lightColor = glm::vec4(glm::vec3(gubo.lightColor) * 0.25f, 1.0f);
			gubo.fogColor = glm::vec4(0.02f, 0.02f, 0.05f, 0.06f);
		}

		//--------- One point light per torch holder with flame ---------
		totalTime += deltaT;
		int nLights = std::min((int)torchPositions.size(), MAX_POINT_LIGHTS);
		// Iterate over each torch and calculate its light contribution
		for (int i = 0; i < nLights; i++)
		{
			// Calculate the flicker effect for the torch light
			float flicker = 0.85f + 0.15f * sinf(totalTime * 7.0f + (float)i * 2.3f);
			gubo.pointLightPos[i] = glm::vec4(torchPositions[i], 4.0f); // w = falloff radius
			gubo.pointLightColor[i] = glm::vec4(3.0f, 1.8f, 0.9f, flicker);
		}
		// Set the remaining point lights to zero to avoid unintended lighting effects
		for (int i = nLights; i < MAX_POINT_LIGHTS; i++)
		{
			gubo.pointLightColor[i] = glm::vec4(0.0f);
		}
		// Map the global uniform buffer object to the GPU memory for the current frame
		DSglobal.map(currentImage, &gubo, 0);
		// Update flame transforms
		for (size_t index = 0; index < DSflames.size(); index++)
		{
			FlameUniformBlock flameUbo{};
			// Calculate the Model-View-Projection matrix for the flame instance
			flameUbo.mvpMat = ViewPrj * flameTransforms[index];
			// Set the animation parameters for the flame (time and index)
			flameUbo.animation = glm::vec4(totalTime, static_cast<float>(index) * 2.3f, 0.0f, 0.0f);
			// Map the flame's uniform buffer to the GPU memory for the current frame
			DSflames[index].map(currentImage, &flameUbo, 0);
		}

		//-------- Update SkyBox uniform buffer (view matrix without translation + dayFactor) --------
		const float FOVy = glm::radians(45.0f);
		const float nearPlane = 0.1f;
		const float farPlane = 400.f;
		glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
		Prj[1][1] *= -1;

		glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(View)); // Rotation only, no translation
		SkyBoxUniformBlock skyUbo{};
		skyUbo.mvpMat = Prj * viewNoTranslation;
		skyUbo.dayFactor = currentDayFactor;
		DSsky.map(currentImage, &skyUbo, 0);

		//----------------------- Uniform Buffers --------------------
		// Defines the local parameters for the uniforms (for each 3D object in the scene)
		UniformBufferObject ubo{};

		// Iterate over all scene instances (scene.I)
		for (int i = 0; i < scene.InstanceCount; i++)
		{
			ubo.mMat = scene.I[i]->Wm;
			ubo.mvpMat = ViewPrj * ubo.mMat;
			ubo.lightParams = instanceParams[i];

			// Set 0: global UBO (lights, camera, fog)
			// Set 1: local UBO (MVP and Model matrices)
			scene.I[i]->DS[0][0]->map(currentImage, &gubo, 0);
			scene.I[i]->DS[0][1]->map(currentImage, &ubo, 0);
		}

		//-------- Update FPS and text ---------
		// Calculates and updates on the screen the FPS (Frame Per Second)
		static float elapsedT = 0.0f; // Accumulated time since the last FPS update
		static int countedFrames = 0; // Number of frames counted since the last FPS update

		countedFrames++;
		elapsedT += deltaT;
		if (elapsedT > 1.0f)
		{												 // Update the FPS display every second
			float Fps = (float)countedFrames / elapsedT; // Calculate the FPS based on the number of frames and elapsed time

			// Prepare the string to display the FPS on the screen
			if (gameManager.currentState == GameState::PLAYING)
			{
				std::ostringstream oss;
				oss << "FPS: " << Fps << "\n";
				txt.print(1.0f, 1.0f, oss.str(), 1, "CO", false, false, true, TAL_RIGHT, TRH_RIGHT, TRV_BOTTOM, {1.0f, 0.0f, 0.0f, 1.0f}, {0.8f, 0.8f, 0.0f, 1.0f});
			}
			else if (txt.Blocks.count(1))
			{
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
	void setupInteractions()
	{
		// Register relics interactions
		// Objects interactions
		interactionManager.addRelicInteraction("Book", "Press E to pick up Sacred Grimoire");
		interactionManager.addRelicInteraction("Cup", "Press E to pick up Holy Chalice");
		interactionManager.addRelicInteraction("Sword", "Press E to pick up Cursed Blade");
		interactionManager.addAltarInteraction("Altar");
		interactionManager.addLockedDoorInteraction("Door_locked", "Key", "Press E to unlock Door");
		interactionManager.addKeyInteraction("Golden_Key", "Key", "Press E to pick up Key");
		// Door interactions
		interactionManager.addDoorInteraction("Door_main");
		interactionManager.setDoorHinge("Door_main", +0.1719f, +1.0f); // + 0.1719 is the local Z-coordinate of the hinge relative to the door model's origin
		interactionManager.addDoorInteraction("Door_L");
		interactionManager.setDoorHinge("Door_L", +0.1719f, +1.0f);
		interactionManager.addDoorInteraction("Door_R");
		interactionManager.setDoorHinge("Door_R", +0.1719f, +1.0f);
		// Statue interactions
		interactionManager.addInfoInteraction("StatueL", "Press E to talk", "Guard: Hello explorer, welcome to the castle!");
		interactionManager.addInfoInteraction("StatueR", "Press E to talk", "Long time ago, the duke of this castle made a pact with the devil to become immortal.");
		interactionManager.saveInitialState(scene);
	}

	// Everything beyond the castle walls: garden, trees and hedges
	void classifyInstances()
	{
		instanceParams.assign(scene.InstanceCount, glm::vec4(0.0f));
		for (int i = 0; i < scene.InstanceCount; i++)
		{
			const std::string &id = *scene.I[i]->id;
			bool outdoor = id.rfind("garden", 0) == 0 ||
						   id.rfind("Tree_", 0) == 0 ||
						   id.rfind("Hedge", 0) == 0 ||
						   id.rfind("Wall", 0) == 0 ||
						   id == "Door_main";

			instanceParams[i] = glm::vec4(outdoor ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	// ----------------- Setup Materials ------------------
	// Sets up the material properties (metallic and roughness) for each instance in the scene based on its ID
	void setupMaterials()
	{
		for (int index = 0; index < scene.InstanceCount; ++index)
		{
			const std::string &id = *scene.I[index]->id;

			float metallic = 0.0f;
			float roughness = 0.8f;

			if (id == "Cup" || id == "Golden_Key")
			{
				metallic = 1.0f;
				roughness = 0.3f;
			}
			else if (id == "Sword")
			{
				metallic = 1.0f;
				roughness = 0.4f;
			}
			else if (id == "Metal_Chest" || id == "Iron_Bucket")
			{
				metallic = 1.0f;
				roughness = 0.55f;
			}
			else if (id == "Forge_Tong" || id == "Anvil" ||
					 id.rfind("Torch_Holder", 0) == 0)
			{
				metallic = 1.0f;
				roughness = 0.65f;
			}
			else if (id.rfind("Jail_", 0) == 0 ||
					 id.rfind("Prison_Wall", 0) == 0)
			{
				metallic = 1.0f;
				roughness = 0.7f;
			}

			instanceParams[index].z = metallic;
			instanceParams[index].w = roughness;
		}
	}

	// Collects the positions and transformation matrices for all torch lights in the scene
	void collectTorchLights()
	{
		torchPositions.clear();
		flameTransforms.clear();

		const glm::vec3 flameBaseLocal(0.0f, 0.12f, 0.15f); // Position in the local space of the torch holder model: where the flame should originate
		const float flameScaleLocal = 0.25f;				// Scale factor for the flame

		// Iterate through all instances in the scene to find torch holders and calculate the world transformations for the flames attached to them
		for (int index = 0; index < scene.InstanceCount; ++index)
		{
			if (scene.I[index]->id->rfind("Torch_Holder", 0) != 0)
			{
				continue;
			}
			// Translate the flame's base position from local space to world space using the torch holder's world matrix
			glm::mat4 flameWorld = glm::translate(scene.I[index]->Wm, flameBaseLocal);
			// Scale the flame according to the local scale factor
			flameWorld = glm::scale(flameWorld, glm::vec3(flameScaleLocal));
			// Store the world transformation for the flame and calculate its position in world space
			flameTransforms.push_back(flameWorld);
			torchPositions.push_back(glm::vec3(flameWorld * glm::vec4(0.0f, 0.24f, 0.0f, 1.0f)));
		}

		std::cout << "Torch lights found: " << torchPositions.size() << "\n";
	}

	// ------------------ GAME LOGIC -------------------
	float GameLogic()
	{
		// Camera FOV-y, Near Plane and Far Plane
		const float FOVy = glm::radians(45.0f); // Field of view in the y direction (in radians)
		const float nearPlane = 0.1f;			// Near clipping plane distance
		const float farPlane = 400.f;			// Far clipping plane distance

		// Retrieve the system input and the current frame time delta to update the camera position and orientation
		float deltaT;										// Time elapsed since the last frame (in seconds)
		glm::vec3 m = glm::vec3(0.0f), r = glm::vec3(0.0f); // m = motion input, r = rotation input
		bool fire = false;									// fire = action input
		getSixAxis(deltaT, m, r, fire);						// Retrieve input from a six-axis controller

		// Update the game timer with the elapsed time since the last frame
		gameManager.updateTimer(deltaT, txt);

		//------------------- 1. Handle Game State Input and UI -------------------
		gameManager.handleInput(window, txt, currentWindowWidth, currentWindowHeight);
		if (gameManager.restartRequested)
		{
			gameManager.restartRequested = false;
			cam.resetCamera();
			interactionManager.reset(scene);
			currentDayFactor = 0.0f;
			victoryMenuTimer = -1.0f;
			txt.removeText(2);
			txt.removeText(3);
		}
		gameManager.updateUI(txt, currentWindowWidth, currentWindowHeight);

		// ------------------ 2. Process mouse/keyboard and Gameplay ------------------
		if (gameManager.currentState == GameState::PLAYING)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Hide and capture the cursor during gameplay

			// Process mouse input to update the camera's orientation based on the current mouse position
			cam.processMouseInput(window);
			cam.processKeyboardInput(window, deltaT, scene);

			interactionManager.updateAnimations(scene, deltaT, cam.getCameraPosition());

			// --------- 3. Check for interactions with objects in the scene ---------
			int nearestInteractableObjIndex = interactionManager.findNearestInteractable(scene, cam, 6.0f, 0.3f); // Find the nearest interactable object
			if (nearestInteractableObjIndex < 0)
			{
				interactionManager.infoTextTimer = 0.0f;
				interactionManager.currentInfoText.clear();
				txt.removeText(3);
			}
			if (nearestInteractableObjIndex >= 0 && interactionManager.infoTextTimer <= 0.0f)
			{
				std::string promptText = interactionManager.getPrompt(nearestInteractableObjIndex, gameManager);
				txt.print(0.0f, 0.75f, promptText, 2, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, getTextScale(currentWindowWidth, currentWindowHeight), getTextScale(currentWindowWidth, currentWindowHeight)); // Display the prompt for the nearest interactable object
			}
			else
			{
				txt.removeText(2); // Remove any previous prompts
			}

			// Handle the 'E' key interaction
			bool ePressedNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
			interactionManager.handleEKey(ePressedNow, nearestInteractableObjIndex, scene, cam, gameManager);

			if (interactionManager.infoTextTimer > 0.0f)
			{
				interactionManager.infoTextTimer -= deltaT; // Decrease the timer for displaying info text
				if (interactionManager.infoTextTimer > 0.0f)
				{
					int actualWidth, actualHeight;
					glfwGetFramebufferSize(window, &actualWidth, &actualHeight);
					float scale = getTextScale(actualWidth, actualHeight);
					float availablePixels = actualWidth * 0.85f;
					float maxWidthUnscaled = availablePixels / scale;
					int maxCharsPerLine = estimateMaxCharsPerLine(actualWidth, scale);
					// std::string wrappedInfoText = wrapText(interactionManager.currentInfoText, maxCharsPerLine);
					std::string wrappedInfoText = wrapTextToWidth(txt, interactionManager.currentInfoText, 4, maxWidthUnscaled);
					txt.print(0.0f, 0.40f, wrappedInfoText, 3, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, scale, scale);
				}
				else
				{
					txt.removeText(3); // Remove the info text when the timer expires
				}
			}

			//-------- Check victory condition --------------
			if (gameManager.curseBroken && !gameManager.victoryTriggered)
			{
				if (victoryMenuTimer < 0.0f)
				{
					if (cam.getCameraPosition().z > 34.0f)
					{
						victoryMenuTimer = 5.0f;
					}
				}
				else
				{
					victoryMenuTimer -= deltaT;
					if (victoryMenuTimer <= 0.0f)
					{
						gameManager.victoryTriggered = true;
						gameManager.clearMenuTexts(txt);
						txt.removeText(2);
						txt.removeText(3);
						gameManager.currentState = GameState::VICTORY;
					}
				}
			}
		}
		else
		{
			// If we are in the menu (not interacting with objects), show the cursor and remove any interaction texts
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			cam.resetMouseTracking();
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
int main()
{
	CursedCastle app; // Create an instance of the application class

	try
	{
		app.run(false); // Run the application, passing 'false' to indicate that ray tracing is not included
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl; // Print the error message to the standard error output
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS; // End the program successfully
}