#pragma once

#include <glad/glad.h>

#include <stdbool.h>

#define GEM_PRINTABLE_ASCII_START 33
#define GEM_PRINTABLE_ASCII_END   127
#define GEM_GLYPH_CNT             (GEM_PRINTABLE_ASCII_END - GEM_PRINTABLE_ASCII_START + 1)
#define GEM_FONT_SIZE             20

typedef struct
{
    float tex_minX, tex_minY;
    float tex_maxX, tex_maxY;
    uint32_t width, height;
    uint32_t xoff, yoff;
} GemGlyphData;

// TODO: Add support for unicode characters, and on-demand
//       glyph loading into a dynamic atlas.
typedef struct
{
    GemGlyphData glyphs[GEM_GLYPH_CNT];
    uint32_t advance;
    GLuint atlas_texture;
} GemFont;

void gem_freetype_init(void);
bool gem_gen_font_atlas(const char* font_path, GemFont* font);
void gem_freetype_cleanup(void);
