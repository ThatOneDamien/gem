#pragma once
#include <structs/quad.h>

#include <glad/glad.h>

#include <stdbool.h>
#include <stddef.h>

#define GEM_PRINTABLE_ASCII_START 33
#define GEM_PRINTABLE_ASCII_END   127
#define GEM_GLYPH_CNT             (GEM_PRINTABLE_ASCII_END - GEM_PRINTABLE_ASCII_START + 1)

typedef struct GemGlyphData GemGlyphData;
typedef struct GemFont      GemFont;

struct GemGlyphData
{
    float    tex_coords[4];
    uint32_t width, height;
    uint32_t xoff, yoff;
};

// TODO: Add support for unicode characters, and on-demand
//       glyph loading into a dynamic atlas.
struct GemFont
{
    GemGlyphData glyphs[GEM_GLYPH_CNT];
    uint32_t advance; // TODO: Change this to be static for the whole program, as all bold and italic fonts should have the same advance
    GLuint atlas_texture;
};

void  freetype_init(void);
bool  gen_font_atlas(const char* font_path, GemFont* font);
void  freetype_cleanup(void);

int   get_font_size(void);
float get_line_height(void);
int   get_vert_advance(void);

void set_font_size(size_t font_size);
void set_line_height(float line_height);
