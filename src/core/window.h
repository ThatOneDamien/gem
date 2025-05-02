#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void gem_window_create(uint32_t width, uint32_t height);
void gem_window_destroy(void);
void gem_window_dispatch_events(void);
void gem_window_swap(void);
void gem_window_get_dims(int* width, int* height);
