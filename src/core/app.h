#pragma once

#include <stdbool.h>
#include <stdint.h>

void gem_app_close(void);
void gem_app_key_press(uint16_t keycode, uint32_t mods);
void gem_app_mouse_press(uint32_t button, uint32_t mods, int x, int y);
void gem_app_request_redraw(void);
bool gem_app_needs_redraw(void);
