#include "renderer.h"
#include "core/core.h"
#include "file/file.h"
#include "font.h"
#include "uniforms.h"

#include <glad/glad.h>

#include <cglm/cglm.h>
#include <cglm/clipspace/ortho_rh_no.h>

#include <string.h>

#define MAX_QUADS    1000
#define MAX_VERTICES (MAX_QUADS * 4)
#define INDEX_COUNT  (MAX_QUADS * 6)

#define TEXT_VERT_SHADER "assets/shaders/basic.vert"
#define TEXT_FRAG_SHADER "assets/shaders/basic.frag"
#define DEFAULT_FONT     "assets/fonts/JetBrainsMono-SemiBold.ttf"

typedef struct
{
    vec2 position;
    vec2 tex_coords;
} TextVertex;

static GLuint s_VAO;
static GLuint s_VBO;
static GLuint s_IBO;
static GLuint s_Shader;
static GemFont s_Font;
static TextVertex* s_VertexData;
static TextVertex* s_VertexInsert;
static uint32_t s_QuadCnt; // Quad count of current batch
static GemRenderStats s_Stats;

static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id);


#ifdef GEM_DEBUG
static APIENTRY void debugCallbackFunc(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*,
                                       const void*);
#endif // GEM_DEBUG

void gem_renderer_init(void)
{
#ifdef GEM_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debugCallbackFunc, NULL);
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_FRAMEBUFFER_SRGB);

    glCreateVertexArrays(1, &s_VAO);
    glCreateBuffers(1, &s_VBO);
    glCreateBuffers(1, &s_IBO);

    glBindVertexArray(s_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TextVertex) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_IBO);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (const void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (const void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    bool result = create_shader_program(TEXT_VERT_SHADER, TEXT_FRAG_SHADER, &s_Shader);
    GEM_ENSURE_MSG(result, "Failed to create text shader, exiting.");
    glUseProgram(s_Shader);

    s_VertexData = malloc(sizeof(TextVertex) * MAX_VERTICES);
    s_VertexInsert = s_VertexData;

    {
        uint32_t indices[INDEX_COUNT];
        for(size_t i = 0, j = 0; i < INDEX_COUNT; i += 6, j += 4)
        {
            indices[i + 0] = j + 0;
            indices[i + 1] = j + 1;
            indices[i + 2] = j + 2;

            indices[i + 3] = j + 2;
            indices[i + 4] = j + 3;
            indices[i + 5] = j + 0;
        }
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }

    result = gem_gen_font_atlas(DEFAULT_FONT, &s_Font);
    GEM_ENSURE_MSG(result, "Failed to create font atlas.");
    glBindTextureUnit(0, s_Font.atlas_texture);

    gem_uniforms_init();

    GEM_ENSURE_ARGS(result, "Failed to create font at path %s.", DEFAULT_FONT);
}

void gem_renderer_cleanup(void)
{
    free(s_VertexData);
    gem_uniforms_cleanup();
    glDeleteProgram(s_Shader);
    glDeleteTextures(1, &s_Font.atlas_texture);
    glDeleteBuffers(1, &s_VBO);
    glDeleteBuffers(1, &s_IBO);
    glDeleteVertexArrays(1, &s_VAO);
}

void gem_renderer_start_batch(void)
{
    s_VertexInsert = s_VertexData;
    s_QuadCnt = 0;
    s_Stats.draw_calls = 0;
    s_Stats.quad_count = 0;
}

void gem_renderer_render_batch(void)
{
    if(s_QuadCnt == 0)
        return;

    glBufferSubData(GL_ARRAY_BUFFER, 0, (uintptr_t)s_VertexInsert - (uintptr_t)s_VertexData, s_VertexData);
    glDrawElements(GL_TRIANGLES, s_QuadCnt * 6, GL_UNSIGNED_INT, NULL);

    s_Stats.quad_count += s_QuadCnt; // Add quad count of current batch to the total
    s_Stats.draw_calls++;
    s_VertexInsert = s_VertexData;
    s_QuadCnt = 0;
}

static const GemGlyphData* get_glyph_data_from_font(const GemFont* font, char c)
{
    size_t index = c >= GEM_PRINTABLE_ASCII_START && c < GEM_PRINTABLE_ASCII_END
                       ? (size_t)c - GEM_PRINTABLE_ASCII_START + 1
                       : 0;
    return font->glyphs[index].width == 0 ? &font->glyphs[0] : &font->glyphs[index];
}

static void draw_str_at(const char* str, size_t count, const GemQuad* bounding_box, float* penX, float* penY)
{
    float pen_X = *penX;
    float pen_Y = *penY;

    for(size_t i = 0; i < count; ++i)
    {
        if(s_QuadCnt == MAX_QUADS)
            gem_renderer_render_batch();
        char c = str[i];

        if(c == '\n')
        {
            pen_X = bounding_box->bottom_left[0] + 10.0f;
            pen_Y -= 20.0f;
        }
        else if(c == ' ')
            pen_X += s_Font.advance;
        else if(pen_X < bounding_box->top_right[0])
        {
            const GemGlyphData* data = get_glyph_data_from_font(&s_Font, c);
            s_VertexInsert[0].position[0] = pen_X + data->xoff;
            s_VertexInsert[0].position[1] = pen_Y + data->yoff - data->height;
            s_VertexInsert[0].tex_coords[0] = data->tex_minX;
            s_VertexInsert[0].tex_coords[1] = data->tex_minY;

            s_VertexInsert[1].position[0] = pen_X + data->xoff + data->width;
            s_VertexInsert[1].position[1] = pen_Y + data->yoff - data->height;
            s_VertexInsert[1].tex_coords[0] = data->tex_maxX;
            s_VertexInsert[1].tex_coords[1] = data->tex_minY;

            s_VertexInsert[2].position[0] = pen_X + data->xoff + data->width;
            s_VertexInsert[2].position[1] = pen_Y + data->yoff;
            s_VertexInsert[2].tex_coords[0] = data->tex_maxX;
            s_VertexInsert[2].tex_coords[1] = data->tex_maxY;

            s_VertexInsert[3].position[0] = pen_X + data->xoff;
            s_VertexInsert[3].position[1] = pen_Y + data->yoff;
            s_VertexInsert[3].tex_coords[0] = data->tex_minX;
            s_VertexInsert[3].tex_coords[1] = data->tex_maxY;

            s_VertexInsert += 4;
            s_QuadCnt++;
            pen_X += s_Font.advance;
        }
    }
    *penX = pen_X;
    *penY = pen_Y;
}

void gem_renderer_draw_buffer(GapBuffer buf, const GemQuad* bounding_box)
{
    GEM_ASSERT(buf != NULL);
    GEM_ASSERT(bounding_box != NULL);

    float pen_X = bounding_box->bottom_left[0] + 10.0f;
    float pen_Y = bounding_box->top_right[1] - (float)GEM_FONT_SIZE;
    size_t gap_pos = gap_get_gap_pos(buf);
    size_t gap_size = gap_get_size(buf);

    if(gap_pos != 0)
        draw_str_at(gap_get_data_before_gap(buf), gap_pos, bounding_box, &pen_X, &pen_Y);
    if(gap_size - gap_pos != 0)
        draw_str_at(gap_get_data_after_gap(buf), gap_size - gap_pos, bounding_box, &pen_X, &pen_Y);
}


static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id)
{
    GLuint program = 0;
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    char* src;
    size_t src_size;
    gem_read_entire_file(vert_path, &src, &src_size);

    glShaderSource(vert, 1, (const GLchar* const*)&src, (const GLint*)&src_size);
    free(src);
    glCompileShader(vert);
    GLint status = 0;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        glDeleteShader(vert);
        GEM_ERROR_ARGS("Failed to compile vertex shader at path %s.", vert_path);
        return false;
    }


    gem_read_entire_file(frag_path, &src, &src_size);
    glShaderSource(frag, 1, (const GLchar* const*)&src, (const GLint*)&src_size);
    free(src);
    glCompileShader(frag);
    status = 0;
    glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        glDeleteShader(vert);
        glDeleteShader(frag);
        GEM_ERROR_ARGS("Failed to compile fragment shader at path %s.", frag_path);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDetachShader(program, vert);
    glDetachShader(program, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(!status)
    {
        glDeleteProgram(program);
        GEM_ERROR("Failed to link shader program.");
        return false;
    }

    *program_id = program;
    return true;
}

const GemRenderStats* gem_renderer_get_stats(void)
{
    return &s_Stats;
}

#ifdef GEM_DEBUG
static APIENTRY void debugCallbackFunc(UNUSED GLenum source, UNUSED GLenum type, UNUSED GLuint id,
                                       GLenum severity, UNUSED GLsizei length,
                                       const GLchar* message, UNUSED const void* userParam)
{
    switch(severity)
    {
    case GL_DEBUG_SEVERITY_LOW:
        fprintf(stderr, "\033[33mOpenGL Warning: %s\033[0m\n", message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        GEM_ERROR_ARGS("OpenGL Error: %s", message);
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        GEM_ERROR_ARGS("OpenGL Critical Error: %s", message);
        break;
    }
}
#endif
