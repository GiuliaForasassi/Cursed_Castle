#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>
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
    EFFECT_OPEN_DOOR, // Open door if you interact with it
    EFFECT_UNLOCK_DOOR, // Unlock door if you have the required key
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

struct AnimatedDoor {
    std::string instanceId;
    enum State { CLOSED, OPENING, STAY_OPEN, CLOSING } state = CLOSED;
    float currentAngle = 0.0f; // degrees
    float targetAngle = 90.0f; // opening of 90 degrees
    float speed = 90.0f; // degrees per second (opens in 1 second)
    float openTimer = 0.0f;
    float openDuration = 5.0f;
    glm::mat4 initialWm = glm::mat4(1.0f);
    bool initialWmSaved = false;
    Collider* originalCollider = nullptr;
    // Cardine in coordinate locali del modello Door (larghezza anta lungo Z, semilarghezza 0.172)
    glm::vec3 hinge = glm::vec3(0.0f, 0.0f, -0.1719f);
    float openSign = 1.0f; // verso di apertura: +1 o -1
    };

struct AnimatedKey {
    std::string instanceId;
    enum State { HIDDEN, APPEARING, COLLECTED } state = HIDDEN;
    float currentScale = 0.0f; // scale for appearing animation
    float targetScale = 1.0f; // final scale when fully appeared
    float speed = 1.0f; // scale per second
    glm::mat4 initialWm = glm::mat4(1.0f);
    bool initialWmSaved = false;
};

// Structure to define the state of a pickup item (e.g., a relic or key) in the scene
struct PickupState {
    glm ::mat4 Wm;
    Collider* C;
};



class InteractionManager {
    // List of interactable objects in the scene
	std::vector<Interactable> interactables; 
    // Flag to check if the 'E' key is pressed for interaction
	bool ePressed = false; 
    std::unordered_set<std::string> inventoryKeys;
    std::unordered_set<std::string> collectedRelics; // Set of collected relics

    // Map to store the state of animated doors in the scene
    std::unordered_map<std::string, AnimatedDoor> doors;

    // Map to store the state of pickup items in the scene
    std::unordered_map<std::string, PickupState> initialPickups; 

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
    // relX in [-1,1]: posizione laterale relativa alla semi-larghezza dell'altare
    void placeOnAltar(Scene& scena, const std::string& id, float relX,
                    float scale = 3.5f, const glm::mat4& rot = glm::mat4(1.0f)) {
        auto altarIt = scena.InstanceIds.find("Altar");
        auto relicIt = scena.InstanceIds.find(id);
        if (altarIt == scena.InstanceIds.end() || relicIt == scena.InstanceIds.end()) return;

        Instance* altar = scena.I[altarIt->second];
        glm::vec3 c = glm::vec3(altar->Wm[3]);
        float topY = c.y, halfW = 0.5f;
        if (altar->C != nullptr) {
            AABBextents E = altar->C->getExtents();
            c.x = (E.xMin + E.xMax) * 0.5f;
            c.z = (E.zMin + E.zMax) * 0.5f;
            topY = E.yMax;
            halfW = (E.xMax - E.xMin) * 0.5f;
        }

        glm::mat4 m = glm::translate(glm::mat4(1.0f),
                        glm::vec3(c.x + relX * halfW * 0.6f, topY + 0.01f, c.z));
        scena.I[relicIt->second]->Wm = m * rot * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    }

    public:
        // Current information text to display when interacting with objects
        std::string currentInfoText = ""; 
        // Timer to control how long the info text is displayed
        float infoTextTimer = 0.0f; 

        // Reset the interaction manager to its initial state, clearing inventory keys, info text, and reactivating all interactables
        void reset(Scene& scena) {
            inventoryKeys.clear();
            collectedRelics.clear();
            currentInfoText = "";
            infoTextTimer = 0.0f;
            for (auto& item : interactables) item.active = true;

            for (const auto& kv : initialPickups) {
                auto it = scena.InstanceIds.find(kv.first);
                if (it == scena.InstanceIds.end()) continue;
                Instance* inst = scena.I[it->second];
                inst->Wm = kv.second.Wm;
                inst->C  = kv.second.C;
            }

            for (auto& pair : doors) {
                AnimatedDoor& door = pair.second;
                door.state = AnimatedDoor::CLOSED;
                door.currentAngle = 0.0f;
                door.openTimer = 0.0f;
                auto it = scena.InstanceIds.find(door.instanceId);
                if (it != scena.InstanceIds.end()) scena.I[it->second]->C = door.originalCollider;
            }
        }

        std::string getPrompt(int index, const GameManager& gm) const {
            if (index < 0 || index >= (int)interactables.size()) return "";
            const Interactable& item = interactables[index];

            if (item.effect == EFFECT_UNLOCK_DOOR) {
                if (doors.count(item.instanceId) && doors.at(item.instanceId).state != AnimatedDoor::CLOSED) {
                    return ""; // Se la porta è già aperta, non mostrare alcun prompt
                }
                if (inventoryKeys.count(item.requiredKey)) {
                    return "Press E to unlock and open Door";
                } else {
                    return "Locked Door (Requires " + item.requiredKey + ")";
                }
            }


            if (item.effect == EFFECT_OPEN_DOOR) {
                if (doors.count(item.instanceId) && doors.at(item.instanceId).state != AnimatedDoor::CLOSED) {
                    return ""; // Se la porta è già aperta, non mostrare prompt
                }
                return "Press E to open Door";
            }

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

        void startOpeningDoor(Scene& scena, const std::string& doorId) {
            auto d = doors.find(doorId);
            if (d == doors.end()) return;
            AnimatedDoor& door = d->second;

            auto it = scena.InstanceIds.find(doorId);
            if (it == scena.InstanceIds.end()) return;

            if (door.state == AnimatedDoor::STAY_OPEN) {
                door.openTimer = door.openDuration; // ri-premendo E rinnovi il tempo
                return;
            }
            if (door.state == AnimatedDoor::CLOSED || door.state == AnimatedDoor::CLOSING) {
                scena.I[it->second]->C = nullptr;
                door.state = AnimatedDoor::OPENING;
            }
        }

        void updateAnimations(Scene& scena, float deltaT, const glm::vec3& playerPos) {
            for (auto& pair : doors) {
                AnimatedDoor& door = pair.second;
                auto it = scena.InstanceIds.find(door.instanceId);
                if (it == scena.InstanceIds.end()) continue;
                Instance* inst = scena.I[it->second];

                if (!door.initialWmSaved) {
                    door.initialWm = inst->Wm;
                    door.originalCollider = inst->C;
                    door.initialWmSaved = true;
                }

                switch (door.state) {
                    case AnimatedDoor::OPENING: {
                        door.currentAngle += door.speed * deltaT;
                        if (door.currentAngle >= door.targetAngle) {
                            door.currentAngle = door.targetAngle;
                            door.state = AnimatedDoor::STAY_OPEN;
                            door.openTimer = door.openDuration; // Inizia il conto alla rovescia
                        }
                        break;
                    }
                    case AnimatedDoor::STAY_OPEN: {
                        door.openTimer -= deltaT;
                        if (door.openTimer <= 0.0f) {
                            door.state = AnimatedDoor::CLOSING; // Tempo scaduto, si richiude
                        }
                        break;
                    }
                    case AnimatedDoor::CLOSING: {
                        glm::vec3 doorPos = glm::vec3(door.initialWm[3]);
                        if (glm::distance(glm::vec2(playerPos.x, playerPos.z),
                                        glm::vec2(doorPos.x, doorPos.z)) < 2.5f) {
                            door.state = AnimatedDoor::STAY_OPEN; // il giocatore è nel vano: rimanda la chiusura
                            door.openTimer = 1.0f;
                            break;
                        }
                        door.currentAngle -= door.speed * deltaT;
                        if (door.currentAngle <= 0.0f) {
                            door.currentAngle = 0.0f;
                            door.state = AnimatedDoor::CLOSED;
                            inst->C = door.originalCollider;
                        }
                        break;
                    }
                    case AnimatedDoor::CLOSED:
                    default:
                        break;
                }

                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(door.currentAngle * door.openSign),glm::vec3(0.0f, 1.0f, 0.0f));
                inst->Wm = door.initialWm * glm::translate(glm::mat4(1.0f), door.hinge) * rot * glm::translate(glm::mat4(1.0f), -door.hinge);
                if (inst->C != nullptr) inst->C->setWorldMatrix(inst->Wm);
            }
        }

        void saveInitialState(Scene& scena) {
            initialPickups.clear();
            for (const auto& item : interactables) {
                if (item.effect != EFFECT_PICKUP_RELIC && item.effect != EFFECT_PICKUP_KEY) 
                    continue;
                auto it = scena.InstanceIds.find(item.instanceId);
                if (it == scena.InstanceIds.end()) 
                    continue;
                initialPickups[item.instanceId] = { scena.I[it->second]->Wm, scena.I[it->second]->C };
            }
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

        void addAltarInteraction(const std::string& altarId, const std::string& prompt = "Press E to place Relic on Altar") {
            interactables.push_back({altarId, "", EFFECT_ALTAR_DEPOSIT, prompt, "", "", true});
        }

        // Function to add a door interaction
        void addDoorInteraction(const std::string& doorId, const std::string& prompt = "Press E to open Door") {
            interactables.push_back({doorId, doorId, EFFECT_OPEN_DOOR, prompt, "", "", true});
            AnimatedDoor door;
            door.instanceId = doorId;
            doors[doorId] = door;
        }

        // Function to add a locked door interaction
        void addLockedDoorInteraction(const std::string& doorId, const std::string& keyId, const std::string& prompt = "Press E to unlock Door") {
            interactables.push_back({doorId, doorId, EFFECT_UNLOCK_DOOR, prompt, "", keyId, true});
            AnimatedDoor door;
            door.instanceId = doorId;
            doors[doorId] = door;
        }

        void setDoorHinge(const std::string& doorId, float localZ, float sign) {
            auto it = doors.find(doorId);
            if (it == doors.end()) return;
            it->second.hinge = glm::vec3(0.0f, 0.0f, localZ);
            it->second.openSign = sign;
        }

         //-------------------- Searching for nearest interactable objects --------------------
        // Function to find the nearest interactable object to the player (camera) within a certain distance threshold
        // Returns the index of the nearest interactable object in the interactables vector, or -1 if none are found within the threshold
        // aimMargin allarga il box per rendere mirabili anche gli oggetti piccoli
        int findNearestInteractable(Scene& scena, const Camera& cam, float maxRange = 4.0f, float aimMargin = 0.15f) {
            const glm::vec3 ro = cam.getCameraPosition();
            const glm::vec3 rd = cam.getCameraFront();

            int bestIndex = -1;
            float bestT = maxRange;

            for (int i = 0; i < (int)interactables.size(); i++) {
                if (!interactables[i].active) continue;

                auto it = scena.InstanceIds.find(interactables[i].instanceId);
                if (it == scena.InstanceIds.end()) 
                    continue;
                Instance* inst = scena.I[it->second];

                AABBextents E;
                if (inst->C != nullptr) {
                    E = inst->C->getExtents();
                } else {
                    // Porta aperta o reliquia già raccolta: nessun collider, ripiego su un box attorno all'origine
                    glm::vec3 p = glm::vec3(inst->Wm[3]);
                    E = {p.x - 0.3f, p.x + 0.3f, p.y - 0.3f, p.y + 0.3f, p.z - 0.3f, p.z + 0.3f};
                }

                E.xMin -= aimMargin; E.xMax += aimMargin;
                E.yMin -= aimMargin; E.yMax += aimMargin;
                E.zMin -= aimMargin; E.zMax += aimMargin;

                float t;
                if (!rayIntersectsAABB(ro, rd, E, t)) 
                    continue;
                if (t > bestT) 
                    continue;

                bestT = t;
                bestIndex = i;
            }
            return bestIndex;
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

                case EFFECT_OPEN_DOOR: {
                    startOpeningDoor(scena, interactable.instanceId);
                    currentInfoText = "Door opened!";
                    infoTextTimer = 2.0f;
                    break;
                }

                case EFFECT_UNLOCK_DOOR: {
                    if (inventoryKeys.count(interactable.requiredKey)) {
                        startOpeningDoor(scena, interactable.instanceId);
                        currentInfoText = "Door unlocked!";
                        infoTextTimer = 2.0f;
                    } else {
                        currentInfoText = "The door is locked! You need the " + interactable.requiredKey + ".";
                        infoTextTimer = 3.0f;
                    }
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
                        if (collectedRelics.count("Book"))  placeOnAltar(scena, "Book",  -1.0f, 3.5f);
                        if (collectedRelics.count("Cup"))   placeOnAltar(scena, "Cup",    0.0f, 3.5f);
                        // la spada nel modello è verticale: va coricata come nella scena
                        if (collectedRelics.count("Sword")) placeOnAltar(scena, "Sword",  1.0f, 3.5f,
                                glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

                        if (gm.relicsPlaced >= gm.TOTAL_RELICS) {
                            gm.curseBroken = true;
                            currentInfoText = "THE CURSE IS BROKEN!\nGo outside and see the sky!";
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

        // Slab test raggio/AABB: tHit = distanza del primo impatto (0 se l'origine è dentro il box)
        static bool rayIntersectsAABB(const glm::vec3& ro, const glm::vec3& rd, const AABBextents& E, float& tHit) {
            const glm::vec3 bmin(E.xMin, E.yMin, E.zMin);
            const glm::vec3 bmax(E.xMax, E.yMax, E.zMax);
            float tmin = 0.0f;
            float tmax = std::numeric_limits<float>::max();

            for (int a = 0; a < 3; a++) {
                if (fabs(rd[a]) < 1e-6f) {
                    if (ro[a] < bmin[a] || ro[a] > bmax[a]) return false; // parallelo allo slab e fuori
                    continue;
                }
                float inv = 1.0f / rd[a];
                float t1 = (bmin[a] - ro[a]) * inv;
                float t2 = (bmax[a] - ro[a]) * inv;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
            tHit = tmin;
            return true;
        }
};