#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void window_create(uint32_t width, uint32_t height);
void window_destroy(void);
void window_dispatch_events(void);
void window_swap(void);
void window_get_dims(int* width, int* height);
