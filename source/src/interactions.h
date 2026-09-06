#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <iostream>
#ifndef SCENE_HPP_GUARD
#define SCENE_HPP_GUARD
#include "modules/Scene.hpp"
#endif
#include "camera.h"
#include "utils.h"
#include "game_state.h"

// Interaction effects that can be applied to interactable objects in the scene
enum InteractionEffects {
    EFFECT_INFO, 
    EFFECT_OPEN_SECRET_DOOR, 
    EFFECT_PICKUP_RELIC, 
    EFFECT_PICKUP_KEY,
    EFFECT_UNLOCK_DOOR,
    EFFECT_ALTAR_DEPOSIT
};
// Structure to define an interactable object in the scene
struct Interactable {
	std::string instanceId; // object you interact with 
	std::string targetId; // object on which the effect will be applied (if empty, it matches the instanceId)
	InteractionEffects effect; // Effect to apply when interacted with
	std::string prompt; // Prompt message to display when the player is near: "Press E for..."
	std::string infoText; // Text to display when the player interacts with the object (if effect is EFFECT_INFO)
    std::string requiredKey; // Key required to interact with this object (in case of EFFECT_UNLOCK_DOOR, or EFFECT_PICKUP_KEY )
	bool active = true; // Whether the interactable is active
};

class InteractionManager {
    // List of interactable objects in the scene
	std::vector<Interactable> interactables; 
    // Flag to check if the 'E' key is pressed for interaction
	bool ePressed = false; 
    std::unordered_set<std::string> inventoryKeys;
    std::unordered_set<std::string> collectedRelics; // Set of collected relics

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

    // Function to hide an instance by moving it far below the ground and disabling its camera component
    void hideInstance(Scene& scena, const std::string& id) {
        auto it = scena.InstanceIds.find(id);
        if (it != scena.InstanceIds.end()) {
            Instance* instance = scena.I[it->second];
            instance->Wm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -50.0f, 0.0f)) * instance->Wm;
            instance->C = nullptr;
        }
    }

    // Posiziona una reliquia sopra l'altare a una posizione specifica
    void placeOnAltar(Scene& scena, const std::string& id, glm::vec3 altarSlotPos, glm::vec3 scale = glm::vec3(4.0f)) {
        auto it = scena.InstanceIds.find(id);
        if (it != scena.InstanceIds.end()) {
            Instance* instance = scena.I[it->second];
            // Crea una nuova matrice di trasformazione posizionata sull'altare
            glm::mat4 m = glm::translate(glm::mat4(1.0f), altarSlotPos);
            m = glm::scale(m, scale);
            instance->Wm = m;
        }
    }

    public:
        // Current information text to display when interacting with objects
        std::string currentInfoText = ""; 
        // Timer to control how long the info text is displayed
        float infoTextTimer = 0.0f; 

        // Reset the interaction manager to its initial state, clearing inventory keys, info text, and reactivating all interactables
        void reset() {
            inventoryKeys.clear();
            collectedRelics.clear();
            currentInfoText = "";
            infoTextTimer = 0.0f;
            for (auto& item : interactables) {
                item.active = true;
            }
        }

        std::string getPrompt(int index, const GameManager& gm) const {
            if (index < 0 || index >= (int)interactables.size()) return "";
            const Interactable& item = interactables[index];

            if (item.effect == EFFECT_ALTAR_DEPOSIT) {
                if (gm.curseBroken) {
                    return "The Altar is glowing (Curse is broken!)";
                }
                if (gm.relicsCollected > gm.relicsPlaced) {
                    return "Press E to place Relic on Altar (" + std::to_string(gm.relicsCollected - gm.relicsPlaced) + " in bag)";
                }
                return "Sacred Altar (Find relics in the castle first)";
            }
            return item.prompt;
        }

        // ------------------ Add different types of interactions ------------------
        // Add an interactable object with the specified ID, prompt, and info text to the list of interactables
	    void addInfoInteraction(const std::string& id, const std::string& prompt, const std::string& text){
		    interactables.push_back({id, "", EFFECT_INFO, prompt, text, "", true}); 
	    }

	    // Add a trigger interaction with the specified ID, target ID, effect, and prompt to the list of interactables
	    void addTriggerInteraction(const std::string& id, const std::string& targetId, InteractionEffects effect, const std::string& prompt){
		    interactables.push_back({id, targetId, effect, prompt, "", "", true});
	    }

        void addRelicInteraction(const std::string& id, const std::string& prompt = "Press E to pick up Sacred Relic") {
            interactables.push_back({id, "", EFFECT_PICKUP_RELIC, prompt, "Sacred Relic collected! Bring it to the Altar.", "", true});
        }

        void addKeyInteraction(const std::string& id, const std::string& keyId, const std::string& prompt = "Press E to pick up Key") {
            interactables.push_back({id, "", EFFECT_PICKUP_KEY, prompt, "Key collected!", keyId, true});
        }

        void addLockedDoorInteraction(const std::string& id, const std::string& targetDoorId, const std::string& keyId, const std::string& prompt = "Press E to unlock door") {
            interactables.push_back({id, targetDoorId, EFFECT_UNLOCK_DOOR, prompt, "", keyId, true});
        }

        void addAltarInteraction(const std::string& altarId, const std::string& prompt = "Press E to place Relic on Altar") {
            interactables.push_back({altarId, "", EFFECT_ALTAR_DEPOSIT, prompt, "", "", true});
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
        void executeInteraction(int interactableIndex, Scene& scena, const Camera& cam, GameManager& gm){

            // 1. Check if the index is valid
            if(interactableIndex < 0 || interactableIndex >= interactables.size())
                return; // Invalid index, do nothing

            // 2. Get the interactable object
            Interactable& interactable = interactables[interactableIndex];

            // 3. Execute the effect based on the type of interaction
            switch(interactable.effect){
                case EFFECT_INFO: {
                    currentInfoText = interactable.infoText; // Set the current info text to display, wrapped to 50 characters per line
                    infoTextTimer = 10.0f; // Set the timer for how long the info text should be displayed (5 seconds)
                    break;
                }

                case EFFECT_PICKUP_RELIC: {
                    hideInstance(scena, interactable.instanceId);
                    interactable.active = false;
                    collectedRelics.insert(interactable.instanceId);
                    gm.relicsCollected++;
                    currentInfoText = "Sacred Relic collected (" + std::to_string(gm.relicsCollected) + "/" + std::to_string(gm.TOTAL_RELICS) + ")! Bring it to the altar.";
                    infoTextTimer = 4.0f;
                    break;
                }

                case EFFECT_PICKUP_KEY: {
                    hideInstance(scena, interactable.instanceId);
                    interactable.active = false;
                    inventoryKeys.insert(interactable.requiredKey);
                    currentInfoText = "Key obtained!";
                    infoTextTimer = 3.0f;
                    break;
                }
                    
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
                        instance->Wm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -15.0f, 0.0f)) * instance->Wm;
                        instance->C = nullptr; // Remove the collider to allow the player to pass through the door
                        interactable.active = false; // Deactivate the interactable to prevent further interactions
                    }
                    break;
                }

                case EFFECT_ALTAR_DEPOSIT: {
                    if (gm.curseBroken) {
                        currentInfoText = "The curse has already been broken!\nEscape the castle to win.";
                        infoTextTimer = 4.0f;
                    } else if (gm.relicsCollected > gm.relicsPlaced) {
                        gm.relicsPlaced = gm.relicsCollected;

                        // Posiziona solo le reliquie che sono state effettivamente raccolte
                        if (collectedRelics.count("Book")) {
                            placeOnAltar(scena, "Book",  glm::vec3(-0.7f, 0.88f, -7.0f), glm::vec3(3.5f)); // Libro a sinistra
                        }
                        if (collectedRelics.count("Cup")) {
                            placeOnAltar(scena, "Cup",   glm::vec3( 0.0f, 0.88f, -7.0f), glm::vec3(3.5f)); // Coppa al centro
                        }
                        if (collectedRelics.count("Sword")) {
                            placeOnAltar(scena, "Sword", glm::vec3( 0.7f, 0.88f, -7.0f), glm::vec3(3.5f)); // Spada a destra
                        }

                        if (gm.relicsPlaced >= gm.TOTAL_RELICS) {
                            gm.curseBroken = true;
                            currentInfoText = "THE CURSE IS BROKEN!\nThe sacred relics resonate on the Altar. Escape the castle!";
                            infoTextTimer = 7.0f;
                        } else {
                            currentInfoText = "Relics placed on the Altar (" + std::to_string(gm.relicsPlaced) + "/" + std::to_string(gm.TOTAL_RELICS) + ")!";
                            infoTextTimer = 4.0f;
                        }
                    } else {
                        currentInfoText = "You have no relics in your bag!\nFind the sacred relics hidden in the castle first.";
                        infoTextTimer = 4.0f;
                    }
                    break;
                }
                default:
                std::cout << "[ERROR] Unknown interaction effect for instance '" << interactable.instanceId << "'\n";
                    break;
            }
        }

        // ----------- Handle the E-key --------
        void handleEKey(bool ePressedNow, int nearestInteractableObjIndex, Scene& scena, const Camera& cam, GameManager& gm) {
            if(ePressedNow && !ePressed && nearestInteractableObjIndex >= 0){
                executeInteraction(nearestInteractableObjIndex, scena, cam, gm); // Execute the interaction with the nearest interactable object
            }
            ePressed = ePressedNow; // Update the state of the 'E' key for the next frame
        }

        //------------------- Other methods ------------------
        const Interactable& get(int index) const { 
            return interactables[index]; 
        }
};