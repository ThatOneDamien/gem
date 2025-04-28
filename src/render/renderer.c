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
#define DEFAULT_FONT     "assets/fonts/JetBrainsMono-Regular.ttf"

typedef struct
{
    vec2 position;
    vec2 tex_coords;
} TextVertex;

static GLuint s_VAO;
static GLuint s_VBO;
static GLuint s_IBO;
static GLuint s_Shader;
static GLuint s_FontAtlas;
static GemFont s_FontData;
static TextVertex* s_VertexData;
static TextVertex* s_VertexInsert;

static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id);
#ifdef GEM_DEBUG
static APIENTRY void debugCallbackFunc(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*,
                                       const void*);
#endif

void gem_renderer_init(void)
{
#ifdef GEM_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debugCallbackFunc, NULL);
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glCreateVertexArrays(1, &s_VAO);
    glCreateBuffers(1, &s_VBO);
    glCreateBuffers(1, &s_IBO);

    glBindVertexArray(s_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TextVertex) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);
    // TextVertex vertices[4] = {
    //     {  { 300.0f, 100.0f }, { 0.0f, 1.0f } },
    //     {  { 600.0f, 100.0f }, { 1.0f, 1.0f } },
    //     {  { 600.0f, 400.0f }, { 1.0f, 0.0f } },
    //     {  { 300.0f, 400.0f }, { 0.0f, 0.0f } }
    // };
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

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

    result = gem_gen_font_atlas(DEFAULT_FONT, &s_FontData, &s_FontAtlas);
    GEM_ENSURE_MSG(result, "Failed to create font atlas.");
    glBindTextureUnit(0, s_FontAtlas);

    gem_uniforms_init();

    GEM_ENSURE_ARGS(result, "Failed to create font at path %s.", DEFAULT_FONT);
}

void gem_draw_str(const char* str, const GemQuad* bounding_box)
{
    s_VertexInsert = s_VertexData;
    float pen_X = bounding_box->bottom_left[0] + 10.0f;
    float pen_Y = bounding_box->top_right[1] - (float)GEM_FONT_SIZE;
    for(const char* p = str; *p; ++p)
    {
        char c = *p;
        if(c != ' ' && c != '\n')
        {
            int index = (int)c - GEM_PRINTABLE_ASCII_START + 1;
            GemGlyphData* data = &s_FontData.glyphs[index];
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
        }
        pen_X += s_FontData.advance;
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, (uintptr_t)s_VertexInsert - (uintptr_t)s_VertexData,
                    s_VertexData);
    glDrawElements(GL_TRIANGLES,
                   ((uintptr_t)s_VertexInsert - (uintptr_t)s_VertexData) / sizeof(TextVertex) / 4 *
                       6,
                   GL_UNSIGNED_INT, NULL);
}

void gem_renderer_cleanup(void)
{
    free(s_VertexData);
    gem_uniforms_cleanup();
    glDeleteProgram(s_Shader);
    glDeleteTextures(1, &s_FontAtlas);
    glDeleteBuffers(1, &s_VBO);
    glDeleteBuffers(1, &s_IBO);
    glDeleteVertexArrays(1, &s_VAO);
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
