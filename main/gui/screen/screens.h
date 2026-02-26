#pragma once

#include "../gui.h"
#include <inttypes.h>

void screens_init();
void screens_on_event(GUIEvent, uint8_t *, unsigned);
void screens_return();
void screens_go_home();