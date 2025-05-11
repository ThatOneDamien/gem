#pragma once

#include <stdint.h>

void gem_app_close(void);
void gem_app_key_press(uint16_t keycode, uint32_t mods);
void gem_app_redraw(void);
