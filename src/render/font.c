#include "font.h"
#include "core/core.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stb_image_write.h>

#include <glad/glad.h>

// TODO: Change this to a dynamic system
#define ATLAS_ROWS 8
#define ATLAS_COLUMNS 12
#define ATLAS_OFFSET 10


static FT_Library s_Library;

void gem_freetype_init(void)
{
    FT_Error err = FT_Init_FreeType(&s_Library);
    GEM_ENSURE_ARGS(err == FT_Err_Ok, "Freetype initialization failed (error code: %d).", err);
}

bool gem_gen_font_atlas(const char* font_path, GemFont* font, GLuint* font_texture_id)
{
    GEM_ASSERT(font_path != NULL);
    GEM_ASSERT(font != NULL);
    GEM_ASSERT(font_texture_id != NULL);
    FT_Face face;
    FT_Error err = FT_New_Face(s_Library, font_path, 0, &face);
    if(err != FT_Err_Ok)
        return false;
    err = FT_Set_Pixel_Sizes(face, GEM_FONT_SIZE, 0);
    if(err != FT_Err_Ok)
        return false;

    FT_Int32 load_flags = FT_LOAD_DEFAULT | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT;
    FT_Int32 render_flags = FT_RENDER_MODE_NORMAL;

    size_t tex_width = ATLAS_COLUMNS * (GEM_FONT_SIZE + 1) + 10;
    size_t tex_height = ATLAS_ROWS * (GEM_FONT_SIZE + 1) + 10;
    uint8_t* pixels = calloc(tex_width * tex_height, 1);

    // Load the 'missing glyph' glyph
    err = FT_Load_Glyph(face, 0, load_flags);
    if(err != FT_Err_Ok)
        return false;
    err = FT_Render_Glyph(face->glyph, render_flags);
    if(err != FT_Err_Ok)
        return false;

    FT_Bitmap* bmp = &face->glyph->bitmap;
    font->glyphs[0] = (GemGlyphData) {
        .tex_minX = (float)ATLAS_OFFSET / (float)tex_width,
        .tex_minY = (float)(tex_height - (ATLAS_OFFSET + bmp->rows)) / (float)tex_height,
        .tex_maxX = (float)(ATLAS_OFFSET + bmp->width) / (float)tex_width,
        .tex_maxY = (float)(tex_height - ATLAS_OFFSET) / (float)tex_height,
        .width = bmp->width,
        .height = bmp->rows,
        .xoff = face->glyph->bitmap_left,
        .yoff = face->glyph->bitmap_top
    };
    font->advance = face->glyph->advance.x >> 6;

    for(uint32_t row = 0; row < bmp->rows; ++row)
        memcpy(pixels + ((ATLAS_OFFSET + row) * tex_width + ATLAS_OFFSET), bmp->buffer + (row * bmp->pitch), bmp->width);

    for(size_t i = 1; i < GEM_GLYPH_CNT; ++i)
    {
        err = FT_Load_Char(face, (char)(i + GEM_PRINTABLE_ASCII_START - 1), load_flags);
        if(err != FT_Err_Ok)
            continue;
        err = FT_Render_Glyph(face->glyph, render_flags);
        if(err != FT_Err_Ok)
            continue;

        bmp = &face->glyph->bitmap;
        size_t xloc = (i % ATLAS_COLUMNS) * (GEM_FONT_SIZE + 1) + ATLAS_OFFSET;
        size_t yloc = (i / ATLAS_COLUMNS) * (GEM_FONT_SIZE + 1) + ATLAS_OFFSET;

        font->glyphs[i] = (GemGlyphData) {
            .tex_minX = (float)xloc / (float)tex_width,
            .tex_minY = (float)yloc / (float)tex_height,
            .tex_maxX = (float)(xloc + bmp->width) / (float)tex_width,
            .tex_maxY = (float)(yloc + bmp->rows)/ (float)tex_height,
            .width = bmp->width,
            .height = bmp->rows,
            .xoff = face->glyph->bitmap_left,
            .yoff = face->glyph->bitmap_top
        };
        font->glyphs[i].c = (char)(i + GEM_PRINTABLE_ASCII_START - 1);

		for(uint32_t row = 0; row < bmp->rows; ++row)
            memcpy(pixels + ((yloc + row) * tex_width + xloc), bmp->buffer + (row * bmp->pitch), bmp->width);
    }

    // stbi_write_bmp("test.bmp", tex_width, tex_height, 1, pixels);
    
    GLuint tex;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_R8, tex_width, tex_height);

    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTextureSubImage2D(tex, 0, 0, 0, tex_width, tex_height, GL_RED, GL_UNSIGNED_BYTE, pixels); 
    *font_texture_id = tex;

    free(pixels);

    return true;
}

void gem_freetype_cleanup(void)
{
    FT_Error err = FT_Done_FreeType(s_Library);
    GEM_ENSURE_ARGS(err == FT_Err_Ok, "Freetype closed unsuccessfully (error code: %d).", err);
}
