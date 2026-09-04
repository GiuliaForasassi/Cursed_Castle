
#pragma once
#include <algorithm>
#include <glm/glm.hpp>
#include <string>
#include <string_view>

//------------------
//- Utility functions for text scaling and wrapping
//------------------
// Function to calculate the text scale based on the current window size. This ensures that the text remains readable across different window sizes.
inline float getTextScale(int windowWidth, int windowHeight){
	float s = std::min(windowWidth/800.0f, windowHeight/600.0f);
	return glm::clamp(s, 0.5f, 2.0f); // Avoid too small or too big text 
}

// Function to wrap text into multiple lines based on a maximum number of characters per line. This helps in displaying text neatly within a constrained width.
inline std::string wrapText(std::string_view text, int maxCharsPerLine = 40) {
    // Initialize the result string that will hold the wrapped text
    std::string result;
    // Reserve memory for the result string to avoid frequent reallocations during text wrapping
    result.reserve(text.size()); 
    // Track the length of the current line 
    int currentLineLen = 0;
    // Start index for the current word in the text
    size_t start = 0;

    while (start < text.size()) {
        // Find the next space or newline character to determine the end of the current word
        size_t end = text.find_first_of(" \n", start);
        bool foundDelimiter = (end != std::string_view::npos);
        
        std::string_view word = text.substr(start, (foundDelimiter ? end - start : text.size() - start));

        // If the current line length + space + word exceeds the limit, wrap to the next line
        if (currentLineLen > 0 && currentLineLen + 1 + static_cast<int>(word.size()) > maxCharsPerLine) {
            result += '\n';
            currentLineLen = 0;
        } else if (currentLineLen > 0) {
            result += ' ';
            currentLineLen++;
        }

        result += word;
        currentLineLen += static_cast<int>(word.size());

        if (!foundDelimiter) break;

        // If there was a newline in the original text, respect it
        if (text[end] == '\n') {
            result += '\n';
            currentLineLen = 0;
        }

        // Move the start index to the character after the space or newline for the next iteration
        start = end + 1;
    }

    return result;
}