#pragma once
#include <string>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifndef TEXTMAKER_HPP_GUARD
#define TEXTMAKER_HPP_GUARD
#include "modules/TextMaker.hpp"
#endif

#include "utils.h"

// Enumeration representing the different states of the game
enum class GameState {
	TITLE, // Title screen
	STORY, // Story screen
	CONTROLS, // Controls screen
	PLAYING, // Gameplay state
    VICTORY // Victory screen
};

class GameManager {
public:
    GameState currentState = GameState::TITLE; // Current state of the game
    GameState previousState = GameState::TITLE; // Previous state of the game

    int relicsCollected = 0; // Number of relics collected by the player
    int relicsPlaced = 0; // Number of relics placed by the player
    const int TOTAL_RELICS = 3; // Total number of relics in the game
    bool curseBroken = false; // Indicates whether the curse has been broken
    bool victoryTriggered = false; // Indicates whether the victory condition has been triggered

    bool enterPressedPrev = false;
    bool tabPressedPrev = false;

    // Method to reset the game state to its initial values
    void reset() {
        currentState = GameState::PLAYING;
        relicsCollected = 0;
        relicsPlaced = 0;
        curseBroken = false;
        victoryTriggered = false;
    }

    // Method to clear all menu-related texts from the screen
    void clearMenuTexts(TextMaker& textMaker) {
        textMaker.removeText(10); // Title
        textMaker.removeText(11); // Message
        textMaker.removeText(12); // Controls instructions
        textMaker.removeText(13); // Extra text / Options
    }

    // Handling of inputs for state changes (ENTER, TAB)
    void handleInput(GLFWwindow* window, TextMaker& txt, int windowWidth, int windowHeight) {
        bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        bool tabPressed   = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;

        switch (currentState) {
            case GameState::TITLE:
                if (enterPressed && !enterPressedPrev) {
                    clearMenuTexts(txt);
                    currentState = GameState::STORY;
                }
                break;
            case GameState::STORY:
                if (enterPressed && !enterPressedPrev) {
                    clearMenuTexts(txt);
                    currentState = GameState::CONTROLS;
                }
                break;
            case GameState::CONTROLS:
                if (tabPressed && !tabPressedPrev) {
                    clearMenuTexts(txt);
                    // If we were just playing, let’s get back to it; otherwise, let’s start playing
                    currentState = GameState::PLAYING;
                }
                break;
            case GameState::PLAYING:
                // With TAB you can open the control screen
                if (tabPressed && !tabPressedPrev) {
                    clearMenuTexts(txt);
                    previousState = GameState::PLAYING;
                    currentState = GameState::CONTROLS;
                }
                break;
            case GameState::VICTORY:
                if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                    clearMenuTexts(txt);
                    reset();
                }
                break;
            
        }
        enterPressedPrev = enterPressed;
        tabPressedPrev = tabPressed;
    }

    void updateUI(TextMaker& txt, int windowWidth, int windowHeight) {
        float scale = getTextScale(windowWidth, windowHeight);

        // Colori a tema dark fantasy / oro
        glm::vec4 goldFill   = {1.0f, 0.85f, 0.4f, 1.0f};
        glm::vec4 whiteFill  = {0.95f, 0.95f, 0.95f, 1.0f};
        glm::vec4 darkStroke = {0.1f, 0.05f, 0.0f, 1.0f};
        glm::vec4 noShadow   = {0.0f, 0.0f, 0.0f, 0.0f};

        switch (currentState) {
            case GameState::TITLE:{
                txt.print(0.5f, 0.35f, "CURSED CASTLE", 10, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, goldFill, darkStroke, noShadow, scale * 1.6f, scale * 1.6f);

                txt.print(0.5f, 0.85f, "PRESS ENTER TO CONTINUE", 12, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_BOTTOM, goldFill, darkStroke, noShadow, scale * 0.9f, scale * 0.9f);
                break;
            }
            case GameState::STORY: {
                txt.print(0.5f, 0.20f, "HAUNTED CASTLE", 10, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, goldFill, darkStroke, noShadow, scale * 1.3f, scale * 1.3f);

                std::string story = "A dark curse plagues the ancient castle,\n"
                                    "trapping the land in eternal shadow.\n\n"
                                    "Find the 3 sacred relics hidden within,\n"
                                    "place them upon the altar, and break the curse\n"
                                    "before escaping to see the dawn!";

                txt.print(0.5f, 0.50f, story, 11, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, whiteFill, darkStroke, noShadow, scale * 0.95f, scale * 0.95f);

                txt.print(0.5f, 0.88f, "PRESS ENTER TO CONTINUE", 12, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_BOTTOM, goldFill, darkStroke, noShadow, scale * 0.85f, scale * 0.85f);
                break;
            }
            case GameState::CONTROLS: {
                txt.print(0.5f, 0.20f, "GAME CONTROLS", 10, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, goldFill, darkStroke, noShadow, scale * 1.4f, scale * 1.4f);

                std::string controls = "[W][A][S][D]   Move\n\n"
                                       "[MOUSE]       Look Around\n\n"
                                       "[E]           Interact / Pick Up / Place\n\n"
                                       "[TAB]         Open / Close Menu";

                txt.print(0.5f, 0.52f, controls, 11, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, whiteFill, darkStroke, noShadow, scale * 1.0f, scale * 1.0f);

                txt.print(0.5f, 0.88f, "PRESS [TAB] TO PLAY", 12, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_BOTTOM, goldFill, darkStroke, noShadow, scale * 0.9f, scale * 0.9f);
                break;
            }
            case GameState::PLAYING: {
                // UI in gioco (es. contatore reliquie in alto a sinistra)
                std::string questInfo = "Relics: " + std::to_string(relicsPlaced) + "/" + std::to_string(TOTAL_RELICS);
                if (curseBroken) {
                    questInfo = "Curse Broken! Escape the castle!";
                }
                txt.print(0.05f, 0.05f, questInfo, 10, "CO", false, false, true,
                          TAL_LEFT, TRH_LEFT, TRV_TOP, goldFill, darkStroke, noShadow, scale * 0.85f, scale * 0.85f);
                break;
            }
            case GameState::VICTORY: {
                txt.print(0.5f, 0.35f, "CURSE BROKEN - YOU WON!", 10, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, goldFill, darkStroke, noShadow, scale * 1.5f, scale * 1.5f);

                txt.print(0.5f, 0.55f, "You successfully escaped the castle under the clear blue sky.\n\n"
                                       "Press [R] to Play Again\n"
                                       "Press [ESC] to Exit", 11, "CO", false, false, true,
                          TAL_CENTER, TRH_CENTER, TRV_MIDDLE, whiteFill, darkStroke, noShadow, scale * 1.0f, scale * 1.0f);
                break;
            }




        }

    }


  
};