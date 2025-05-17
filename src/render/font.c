#include "font.h"
#include "core/core.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stb_image_write.h>

#include <glad/glad.h>

#include <math.h>

// TODO: Change this to a dynamic system
#define ATLAS_ROWS    8
#define ATLAS_COLUMNS 12
#define ATLAS_OFFSET  10

#define DEFAULT_FONT_SIZE   20
#define DEFAULT_LINE_HEIGHT 1.2f
#define DEFAULT_VERT_ADV    24.0f

#define CHECK_FT_OR_RET(to_check, ret_val) \
    if((to_check) != FT_Err_Ok)            \
        return ret_val;
#define CHECK_FT_OR_CONTINUE(to_check) \
    if((to_check) != FT_Err_Ok)        \
        continue;
#define CHECK_FT_OR_GOTO(to_check, label) \
    if((to_check) != FT_Err_Ok)           \
        goto label;

static const FT_Int32 LOAD_FLAGS = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
static const FT_Int32 RENDER_FLAGS = FT_RENDER_MODE_NORMAL;

static FT_Library s_Library;
static size_t s_FontSize = DEFAULT_FONT_SIZE; // For now only one font size is allowed globally
static float s_LineHeight = DEFAULT_LINE_HEIGHT;
static float s_VertAdvance = DEFAULT_VERT_ADV;

void gem_freetype_init(void)
{
    FT_Error err = FT_Init_FreeType(&s_Library);
    GEM_ENSURE_ARGS(err == FT_Err_Ok, "Freetype initialization failed (error code: %d).", err);
}

static bool get_cell_dims(FT_Face face, size_t* width, size_t* height)
{
    *width = 0;
    *height = 0;

    CHECK_FT_OR_RET(FT_Load_Glyph(face, 0, LOAD_FLAGS), false);

    *width = face->glyph->bitmap.width;
    *height = face->glyph->bitmap.rows;

    for(size_t i = GEM_PRINTABLE_ASCII_START; i < GEM_PRINTABLE_ASCII_END; ++i)
    {
        CHECK_FT_OR_CONTINUE(FT_Load_Char(face, i, LOAD_FLAGS));
        if(*width < face->glyph->bitmap.width)
            *width = face->glyph->bitmap.width;
        if(*height < face->glyph->bitmap.rows)
            *height = face->glyph->bitmap.rows;
    }

    return true;
}

GemGlyphData put_glyph_in_atlas(uint8_t* atlas, size_t tex_width, size_t tex_height,
                                FT_GlyphSlot glyph, size_t xloc, size_t yloc)
{
    FT_Bitmap* bmp = &glyph->bitmap;
    float x = (float)xloc;
    float y = (float)yloc;
    float tw = (float)tex_width;
    float th = (float)tex_height;

    GemGlyphData glyph_data = (GemGlyphData) { 
        .width = bmp->width,
        .height = bmp->rows,
        .xoff = glyph->bitmap_left,
        .yoff = glyph->bitmap_top,
        .tex_coords = make_quad(x / tw,
                                (y + (float)bmp->rows) / th,
                                (x + bmp->width) / tw,
                                y / th)
    };

    for(uint32_t row = 0; row < bmp->rows; ++row)
        memcpy(atlas + ((yloc + row) * tex_width + xloc), bmp->buffer + (row * bmp->pitch),
               bmp->width);
    return glyph_data;
}

bool gem_gen_font_atlas(const char* font_path, GemFont* font)
{
    GEM_ASSERT(font_path != NULL);
    GEM_ASSERT(font != NULL);

    memset(font, 0, sizeof(GemFont));
    FT_Face face;
    bool result = false;
    CHECK_FT_OR_RET(FT_New_Face(s_Library, font_path, 0, &face), false);
    CHECK_FT_OR_GOTO(FT_Set_Pixel_Sizes(face, s_FontSize, 0), clean);

    size_t cell_width, cell_height;
    if(!get_cell_dims(face, &cell_width, &cell_height))
        goto clean;

    cell_width += 1; // Padding
    cell_height += 1;

    size_t tex_width = ATLAS_COLUMNS * cell_width + ATLAS_OFFSET;
    size_t tex_height = ATLAS_ROWS * cell_height + ATLAS_OFFSET;

    // Load the 'missing glyph' glyph
    CHECK_FT_OR_GOTO(FT_Load_Glyph(face, 0, LOAD_FLAGS), clean);
    CHECK_FT_OR_GOTO(FT_Render_Glyph(face->glyph, RENDER_FLAGS), clean);

    uint8_t* pixels = calloc(tex_width * tex_height, 1);
    GEM_ENSURE(pixels != NULL);

    font->advance = face->glyph->advance.x >> 6;
    font->glyphs[0] = put_glyph_in_atlas(pixels, tex_width, tex_height, face->glyph, ATLAS_OFFSET, ATLAS_OFFSET);

    for(size_t i = 1; i < GEM_GLYPH_CNT; ++i)
    {
        CHECK_FT_OR_CONTINUE(FT_Load_Char(face, i + GEM_PRINTABLE_ASCII_START - 1, LOAD_FLAGS));
        CHECK_FT_OR_CONTINUE(FT_Render_Glyph(face->glyph, RENDER_FLAGS));

        size_t xloc = (i % ATLAS_COLUMNS) * cell_width + ATLAS_OFFSET;
        size_t yloc = (i / ATLAS_COLUMNS) * cell_height + ATLAS_OFFSET;

        font->glyphs[i] = put_glyph_in_atlas(pixels, tex_width, tex_height, face->glyph, xloc, yloc);
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

size_t gem_get_font_size(void)
{
    return s_FontSize;
}

float gem_get_line_height(void)
{
    return s_LineHeight;
}

float gem_get_vert_advance(void)
{
    return s_VertAdvance;
}

void gem_set_font_size(size_t font_size)
{
    s_FontSize = font_size;
    // This should also recreate the atlases with the new
    // font size, but in reality this is not a good way 
    // of doing this at all, so this is temporary.
    s_VertAdvance = roundf((float)s_FontSize * s_LineHeight);
}

void gem_set_line_height(float line_height)
{
    s_LineHeight = line_height;
    s_VertAdvance = roundf((float)s_FontSize * s_LineHeight);
}
