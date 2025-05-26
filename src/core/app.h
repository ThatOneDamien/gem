#pragma once

#include <stdbool.h>
#include <stdint.h>

void gem_close(int err);
void gem_key_press(uint16_t keycode, uint32_t mods);
void gem_mouse_press(uint32_t button, uint32_t mods, int x, int y);
void gem_request_redraw(void);
bool gem_needs_redraw(void);
