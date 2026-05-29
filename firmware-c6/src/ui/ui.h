#pragma once
#include "lvgl.h"
#include "../../state.h"

extern AppState g_state;     // defined in firmware-c6.ino

void ui_init();              // builds widgets; call after lvgl_port_init
void ui_apply_state();       // call after mutating g_state to refresh widgets
