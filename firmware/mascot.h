#pragma once
#include <LovyanGFX.hpp>
#include "state.h"

// Draws the mascot into the given canvas sprite within the rect (x,y,w,h).
// Uses millis() internally for animation timing.
void drawMascot(LGFX_Sprite& g, MascotState state, int x, int y, int w, int h);

// Returns true if the state needs continuous redraws (animation).
bool mascotAnimating(MascotState state);

// Animation tick interval in ms for the state (0 = no animation).
uint32_t mascotAnimInterval(MascotState state);
