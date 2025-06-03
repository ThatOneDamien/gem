#pragma once

#include <stdint.h>

typedef struct vec4color vec4color;
struct vec4color
{
    float r;
    float g;
    float b;
    float a;
};

static inline vec4color rgb_int_to_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    vec4color res;
    res.r = (float)r / 255.0f;
    res.g = (float)g / 255.0f;
    res.b = (float)b / 255.0f;
    res.a = (float)a / 255.0f;
    return res;
}

static inline vec4color hex_rgba_to_color(uint32_t hex)
{
    vec4color res;
    res.a = (float)((hex >>  0) & 0xFF) / 255.0f;
    res.b = (float)((hex >>  8) & 0xFF) / 255.0f;
    res.g = (float)((hex >> 16) & 0xFF) / 255.0f;
    res.r = (float)((hex >> 24) & 0xFF) / 255.0f;
    return res;
}

