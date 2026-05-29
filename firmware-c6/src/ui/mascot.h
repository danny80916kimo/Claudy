#pragma once
#include "lvgl.h"
#include "../../state.h"

// Create the mascot canvas as a child of `parent`, positioned per theme.h.
// Allocates the canvas pixel buffer (MASCOT_W * MASCOT_H * 2 bytes).
// Returns the canvas object.
lv_obj_t* mascot_create(lv_obj_t *parent);

// Redraw the mascot for the given state. Call from your animation timer.
void mascot_draw(lv_obj_t *canvas, MascotState state);

// Animation interval (ms) for the given state. 0 = static (no redraw needed).
uint32_t mascot_anim_interval(MascotState state);
