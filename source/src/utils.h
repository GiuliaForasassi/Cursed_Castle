
#pragma once
#include <algorithm>
#include <glm/glm.hpp>
#include <string>
#include <string_view>

#ifndef TEXTMAKER_HPP_GUARD
#define TEXTMAKER_HPP_GUARD
#include "modules/TextMaker.hpp"
#endif

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

// Wraps text so each line's *real* rendered pixel width (misurata con i metrics del font) sta entro maxWidthPixels
inline std::string wrapTextToWidth(TextMaker& txt, std::string_view text, int fontId, float maxWidthPixels) {
    auto lineWidth = [&](const std::string& line) -> int {
        int w = 0, h = 0, nlines = 0, totChars = 0;
        std::vector<int> linew; std::vector<std::string> lines;
        txt.measureText(line, fontId, w, h, nlines, totChars, linew, lines);
        return w;
    };

    std::string result, currentLine;
    size_t start = 0;

    while (start < text.size()) {
        size_t end = text.find_first_of(" \n", start);
        bool found = (end != std::string_view::npos);
        std::string_view word = text.substr(start, found ? end - start : text.size() - start);

        std::string candidate = currentLine.empty() ? std::string(word) : currentLine + " " + std::string(word);
        if (!currentLine.empty() && lineWidth(candidate) > maxWidthPixels) {
            result += currentLine + '\n';
            currentLine = std::string(word);
        } else {
            currentLine = candidate;
        }

        if (!found) break;
        if (text[end] == '\n') {
            result += currentLine + '\n';
            currentLine.clear();
        }
        start = end + 1;
    }
    result += currentLine;
    return result;
}

// Function to estimate the maximum number of characters per line based on the window width, text scale, and other parameters. This helps in determining how to wrap text appropriately.
inline int estimateMaxCharsPerLine(int windowWidth, float textScale, float marginFraction = 0.85f, float avgCharWidthAtScale1 = 18.0f) {
    float availablePixels = windowWidth * marginFraction;
    float charWidth = avgCharWidthAtScale1 * textScale;
    int estimated =  (int)(availablePixels / charWidth);
    return std::max(10, estimated - 3);
}