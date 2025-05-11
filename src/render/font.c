#include "font.h"
#include "core/core.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stb_image_write.h>

#include <glad/glad.h>

// TODO: Change this to a dynamic system
#define ATLAS_ROWS    8
#define ATLAS_COLUMNS 12
#define ATLAS_OFFSET  10

#define CHECK_FT_OR_RET(to_check, ret_val) \
    if((to_check) != FT_Err_Ok)            \
        return ret_val;
#define CHECK_FT_OR_CONTINUE(to_check) \
    if((to_check) != FT_Err_Ok)        \
        continue;
#define CHECK_FT_OR_GOTO(to_check, label) \
    if((to_check) != FT_Err_Ok)           \
        goto label;

static FT_Library s_Library;

void gem_freetype_init(void)
{
    FT_Error err = FT_Init_FreeType(&s_Library);
    GEM_ENSURE_ARGS(err == FT_Err_Ok, "Freetype initialization failed (error code: %d).", err);
}

static bool get_cell_dims(FT_Face face, FT_Int32 load_flags, size_t* width, size_t* height)
{
    *width = 0;
    *height = 0;

    CHECK_FT_OR_RET(FT_Load_Glyph(face, 0, load_flags), false);

    *width = face->glyph->bitmap.width;
    *height = face->glyph->bitmap.rows;

    for(size_t i = GEM_PRINTABLE_ASCII_START; i < GEM_PRINTABLE_ASCII_END; ++i)
    {
        CHECK_FT_OR_CONTINUE(FT_Load_Char(face, i, load_flags));
        if(*width < face->glyph->bitmap.width)
            *width = face->glyph->bitmap.width;
        if(*height < face->glyph->bitmap.rows)
            *height = face->glyph->bitmap.rows;
    }

    return true;
}

bool gem_gen_font_atlas(const char* font_path, GemFont* font)
{
    GEM_ASSERT(font_path != NULL);
    GEM_ASSERT(font != NULL);

    FT_Face face;
    bool result = false;
    CHECK_FT_OR_RET(FT_New_Face(s_Library, font_path, 0, &face), false);
    CHECK_FT_OR_GOTO(FT_Set_Pixel_Sizes(face, GEM_FONT_SIZE, 0), clean);

    const FT_Int32 load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
    const FT_Int32 render_flags = FT_RENDER_MODE_NORMAL;

    size_t cell_width, cell_height;
    if(!get_cell_dims(face, load_flags, &cell_width, &cell_height))
        goto clean;
    cell_width += 1; // Padding
    cell_height += 1;
    size_t tex_width = ATLAS_COLUMNS * cell_width + ATLAS_OFFSET;
    size_t tex_height = ATLAS_ROWS * cell_height + ATLAS_OFFSET;

    // Load the 'missing glyph' glyph
    CHECK_FT_OR_GOTO(FT_Load_Glyph(face, 0, load_flags), clean);
    CHECK_FT_OR_GOTO(FT_Render_Glyph(face->glyph, render_flags), clean);

    uint8_t* pixels = calloc(tex_width * tex_height, 1);
    GEM_ENSURE(pixels != NULL);

    memset(font, 0, sizeof(GemFont));

    FT_Bitmap* bmp = &face->glyph->bitmap;
    font->glyphs[0] = (GemGlyphData) { 
        .tex_coords = {
            .bottom_left = { 
                (float)ATLAS_OFFSET / (float)tex_width, 
                (float)(ATLAS_OFFSET + bmp->rows) / (float)tex_height
            },
            .top_right = { 
                (float)(ATLAS_OFFSET + bmp->width) / (float)tex_width,
                (float)ATLAS_OFFSET / (float)tex_height 
            }
        },
        .width = bmp->width,
        .height = bmp->rows,
        .xoff = face->glyph->bitmap_left,
        .yoff = face->glyph->bitmap_top 
    };
    font->advance = face->glyph->advance.x >> 6;

    for(uint32_t row = 0; row < bmp->rows; ++row)
        memcpy(pixels + ((ATLAS_OFFSET + row) * tex_width + ATLAS_OFFSET),
               bmp->buffer + (row * bmp->pitch), bmp->width);

    for(size_t i = 1; i < GEM_GLYPH_CNT; ++i)
    {
        CHECK_FT_OR_CONTINUE(FT_Load_Char(face, i + GEM_PRINTABLE_ASCII_START - 1, load_flags));
        CHECK_FT_OR_CONTINUE(FT_Render_Glyph(face->glyph, render_flags));

        bmp = &face->glyph->bitmap;
        size_t xloc = (i % ATLAS_COLUMNS) * cell_width + ATLAS_OFFSET;
        size_t yloc = (i / ATLAS_COLUMNS) * cell_height + ATLAS_OFFSET;

        font->glyphs[i] = (GemGlyphData) { 
            .tex_coords = {
                .bottom_left = { 
                    (float)xloc / (float)tex_width, 
                    (float)(yloc + bmp->rows) / (float)tex_height
                },
                .top_right = { 
                    (float)(xloc + bmp->width) / (float)tex_width,
                    (float)yloc / (float)tex_height 
                }
            },
            .width = bmp->width,
            .height = bmp->rows,
            .xoff = face->glyph->bitmap_left,
            .yoff = face->glyph->bitmap_top 
        };

        for(uint32_t row = 0; row < bmp->rows; ++row)
            memcpy(pixels + ((yloc + row) * tex_width + xloc), bmp->buffer + (row * bmp->pitch),
                   bmp->width);
    }

    // stbi_write_bmp("test.bmp", tex_width, tex_height, 1, pixels);

    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glCreateTextures(GL_TEXTURE_2D, 1, &font->atlas_texture);
    glTextureStorage2D(font->atlas_texture, 1, GL_R8, tex_width, tex_height);

    glTextureParameteri(font->atlas_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(font->atlas_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureParameteri(font->atlas_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(font->atlas_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTextureSubImage2D(font->atlas_texture, 0, 0, 0, tex_width, tex_height, GL_RED, GL_UNSIGNED_BYTE, pixels);


    free(pixels);
    result = true;

clean:
    CHECK_FT_OR_RET(FT_Done_Face(face), false);
    return result;
}

void gem_freetype_cleanup(void)
{
    FT_Error err = FT_Done_FreeType(s_Library);
    GEM_ENSURE_ARGS(err == FT_Err_Ok, "Freetype closed unsuccessfully (error code: %d).", err);
}
