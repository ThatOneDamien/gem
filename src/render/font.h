#pragma once
#include <structs/quad.h>

#include <glad/glad.h>

#include <stdbool.h>

#define GEM_PRINTABLE_ASCII_START 33
#define GEM_PRINTABLE_ASCII_END   127
#define GEM_GLYPH_CNT             (GEM_PRINTABLE_ASCII_END - GEM_PRINTABLE_ASCII_START + 1)
#define GEM_FONT_SIZE             20

typedef struct
{
    GemQuad  tex_coords;
    uint32_t width, height;
    uint32_t xoff, yoff;
} GemGlyphData;

// TODO: Add support for unicode characters, and on-demand
//       glyph loading into a dynamic atlas.
typedef struct
{
    GemGlyphData glyphs[GEM_GLYPH_CNT];
    uint32_t advance; // TODO: Change this to be static for the whole program, as all bold and italic fonts should have the same advance
    GLuint atlas_texture;
} GemFont;

void gem_freetype_init(void);
bool gem_gen_font_atlas(const char* font_path, GemFont* font);
void gem_freetype_cleanup(void);
