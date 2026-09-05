#pragma once
#ifndef SCENE_HPP_GUARD
#define SCENE_HPP_GUARD
#include "modules/Scene.hpp"
#endif
#include "camera.h"
#include "utils.h"

// Interaction effects that can be applied to interactable objects in the scene
enum InteractionEffects {EFFECT_INFO, EFFECT_OPEN_SECRET_DOOR, EFFECT_TOGGLE_CANDLE, EFFECT_PICKUP};
// Structure to define an interactable object in the scene
struct Interactable {
	std::string instanceId; // object you interact with 
	std::string targetId; // object on which the effect will be applied (if empty, it matches the instanceId)
	InteractionEffects effect; // Effect to apply when interacted with
	std::string prompt; // Prompt message to display when the player is near: "Press E for..."
	std::string infoText; // Text to display when the player interacts with the object (if effect is EFFECT_INFO)

	bool active = true; // Whether the interactable is active
};

class InteractionManager {
    // List of interactable objects in the scene
	std::vector<Interactable> interactables; 
    // Flag to check if the 'E' key is pressed for interaction
	bool ePressed = false; 

    //----------------- Helper functions for getting instance world position -------------------
    // Function to get the world position of an instance based on its ID
	glm::vec3 getInstanceWorldPosition(Scene& scena, const Camera& cam, const std::string& instanceId) {
		// 1. Find if the instance exists in the map
		auto it = scena.InstanceIds.find(instanceId); // Look for the instance ID in the map SC.InstanceIds)
		// 2. If the ID is not found, return a secure far away position (to avoid collisions)
		if (it == scena.InstanceIds.end()) {
			glm::vec3 farAwayPos = cam.getCameraPosition() + glm::vec3(1000000.0f);
			return farAwayPos;
		}
		// 3. If the ID is found, retrieve the instance 
		int instanceIndex = it->second;
		Instance* instance = scena.I[instanceIndex];

		// 4. Get the world matrix of the instance and extract its translation (position) component
		const glm::mat4& worldMatrix = instance->Wm; // Get the world matrix of the instance
		glm::vec3 worldPosition = glm::vec3(worldMatrix[3]); // Extract the translation component (position) from the world matrix
		return worldPosition;	
	}

    public:
        // Current information text to display when interacting with objects
        std::string currentInfoText = ""; 
        // Timer to control how long the info text is displayed
        float infoTextTimer = 0.0f; 

        // Add an interactable object with the specified ID, prompt, and info text to the list of interactables
	    void addInfoInteraction(const std::string& id, const std::string& prompt, const std::string& text){
		interactables.push_back({id, "", EFFECT_INFO, prompt, text, true}); 
	    }

	    // Add a trigger interaction with the specified ID, target ID, effect, and prompt to the list of interactables
	    void addTriggerInteraction(const std::string& id, const std::string& targetId, InteractionEffects effect, const std::string& prompt){
		interactables.push_back({id, targetId, effect, prompt, "", true});
	    }

         //-------------------- Searching for nearest interactable objects --------------------
        // Function to find the nearest interactable object to the player (camera) within a certain distance threshold
        // Returns the index of the nearest interactable object in the interactables vector, or -1 if none are found within the threshold
        int findNearestInteractable(Scene& scena, const Camera& cam, float maxRange=6.0f, float maxAngleDegrees=60.0f){
            int nearestIndex = -1; // Initialize the index of the nearest interactable to -1 (not found)
            float nearestDistance = maxRange; // Initialize the nearest distance to the maximum range
            
            for(int i = 0; i < interactables.size(); i++){
                if(!interactables[i].active) 
                    continue; // Skip inactive interactables
                glm::vec3 interactablePos = getInstanceWorldPosition(scena, cam, interactables[i].instanceId); // Get the world position of the interactable
                glm::vec3 toInteractable = interactablePos - cam.getCameraPosition(); // Calculate the vector from the camera to the interactable
                float distance = glm::length(toInteractable); // Calculate the distance to the interactable
                if(distance > nearestDistance)
                    continue; // Skip if the distance is greater than the nearest distance found so far
                if(distance > 0.001f){
                    glm::vec3 directionToInteractable = toInteractable / distance; // Normalize the vector to get the direction
                    // Check if the interactable is within the maximum angle threshold relative to the camera's front direction
                    if(glm::dot(directionToInteractable, cam.getCameraFront()) < cos(glm::radians(maxAngleDegrees)))
                        continue;
                }
                nearestIndex = i; // Update the index of the nearest interactable
                nearestDistance = distance; // Update the nearest distance
            }
            return nearestIndex; // Return the index of the nearest interactable 
        }
        

        //-------------------- Execution mechanism for interactions --------------------
        // TODO: Finish to implement this function
        // Function to execute the interaction with the interactable object
        void executeInteraction(int interactableIndex, Scene& scena, const Camera& cam){
            // 1. Check if the index is valid
            if(interactableIndex < 0 || interactableIndex >= interactables.size())
                return; // Invalid index, do nothing
            // 2. Get the interactable object
            Interactable& interactable = interactables[interactableIndex];
            // 3. Execute the effect based on the type of interaction
            switch(interactable.effect){
                case EFFECT_INFO:
                    currentInfoText = interactable.infoText; // Set the current info text to display, wrapped to 50 characters per line
                    infoTextTimer = 10.0f; // Set the timer for how long the info text should be displayed (5 seconds)
                    break;
                case EFFECT_OPEN_SECRET_DOOR: {
                    // Determine the target ID for the interaction. If the target ID is empty, use the instance ID of the interactable object
                    std::string targetId;
                    if(interactable.targetId.empty())
                        targetId = interactable.instanceId; // If no target ID is specified, use the instance ID of the interactable
                    else
                        targetId = interactable.targetId; // Use the specified target ID
                    
                    // Find the instance in the scene based on the target ID
                    auto it = scena.InstanceIds.find(targetId);
                    if(it != scena.InstanceIds.end()){
                        int instanceIndex = it->second; // Get the index of the instance in the scene
                        Instance *instance = scena.I[instanceIndex]; // Get the instance object
                        if(instance->C == nullptr){
                            // If the secret door was opened in another interaction, don't open it again
                            interactable.active = false; // Deactivate the interactable to prevent further interactions
                            break;
                        }

                        // Apply the effect
                        // TODO: change this number
                        const float SINK_DEPTH = 15.0f; // Depth to sink the door into the ground
                        instance->Wm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -SINK_DEPTH, 0.0f)) * instance->Wm;
                        instance->C = nullptr; // Remove the collider to allow the player to pass through the door
                        interactable.active = false; // Deactivate the interactable to prevent further interactions
                    }
                    break;
                }
                default:
                std::cout << "[ERROR] Unknown interaction effect for instance '" << interactable.instanceId << "'\n";
                    break;
            }
        }

        // ----------- Handle the E-key --------
        void handleEKey(bool ePressedNow, int nearestInteractableObjIndex, Scene& scena, const Camera& cam) {
            if(ePressedNow && !ePressed && nearestInteractableObjIndex >= 0){
                executeInteraction(nearestInteractableObjIndex, scena, cam); // Execute the interaction with the nearest interactable object
            }
            ePressed = ePressedNow; // Update the state of the 'E' key for the next frame
        }

        //------------------- Other methods ------------------
        const Interactable& get(int index) const { 
            return interactables[index]; 
        }
};