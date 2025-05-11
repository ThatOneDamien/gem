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
    vec2  position;
    vec2  tex_coords;
    vec4  color;
    float solid;
} QuadVertex;

static GLuint s_VAO;
static GLuint s_VBO;
static GLuint s_IBO;
static GLuint s_Shader;
static GemFont s_Font;
static QuadVertex* s_VertexData;
static QuadVertex* s_VertexInsert;
static uint32_t s_QuadCnt; // Quad count of current batch
static GemRenderStats s_Stats;

static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id);
static void draw_quad(const GemQuad* quad, const GemQuad* tex, const vec4 color, bool is_solid);

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
    glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertex) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_IBO);

    // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (const void*)0);
    glEnableVertexAttribArray(0);

    // Tex Coords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (const void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);

    // Color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (const void*)(sizeof(float) * 4));
    glEnableVertexAttribArray(2);

    // Solid
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (const void*)(sizeof(float) * 8));
    glEnableVertexAttribArray(3);

    bool result = create_shader_program(TEXT_VERT_SHADER, TEXT_FRAG_SHADER, &s_Shader);
    GEM_ENSURE_MSG(result, "Failed to create text shader, exiting.");
    glUseProgram(s_Shader);

    s_VertexData = calloc(MAX_VERTICES, sizeof(QuadVertex));
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

void gem_renderer_draw_str(const char* str, const GemQuad* bounding_box)
{
    float penX = bounding_box->bottom_left[0];
    float penY = bounding_box->top_right[1];
    gem_renderer_draw_str_at(str, strlen(str), bounding_box, &penX, &penY);
}

static const GemGlyphData* get_glyph_data_from_font(const GemFont* font, char c)
{
    size_t index = c >= GEM_PRINTABLE_ASCII_START && c < GEM_PRINTABLE_ASCII_END
                       ? (size_t)c - GEM_PRINTABLE_ASCII_START + 1
                       : 0;
    return font->glyphs[index].width == 0 ? &font->glyphs[0] : &font->glyphs[index];
}

static void draw_char(char c, float x, float y)
{
    const GemGlyphData* data = get_glyph_data_from_font(&s_Font, c);
    x += data->xoff;
    y += data->yoff;
    GemQuad quad = {
        .bottom_left = { x, y - data->height },
        .top_right = { x + data->width, y }
    };
    vec4 color = { 0.7f, 0.4f, 0.9f, 1.0f };
    draw_quad(&quad, &data->tex_coords, color, false);
}

static void handle_char(char c, const GemQuad* bounding_box, float* penX, float* penY)
{
    if(c == '\n')
    {
        *penX = bounding_box->bottom_left[0];
        *penY -= (float)GEM_FONT_SIZE * 1.2f;
    }
    else if(c == ' ')
        *penX += s_Font.advance;
    else if(c == '\t')
        *penX += s_Font.advance * 4; // TODO: Change this to actually be correct lmao.
    else if(*penX < bounding_box->top_right[0])
    {
        draw_char(c, *penX, *penY);
        *penX += s_Font.advance;
    }
}

void gem_renderer_draw_str_at(const char* str, size_t count, const GemQuad* bounding_box, float* penX, float* penY)
{
    *penY -= GEM_FONT_SIZE;

    for(size_t i = 0; i < count && *penY > bounding_box->bottom_left[1]; ++i)
        handle_char(str[i], bounding_box, penX, penY);

    *penY += GEM_FONT_SIZE;
}

void gem_renderer_draw_buffer(const TextBuffer* buffer)
{
    GEM_ASSERT(buffer != NULL);

    GemQuad line_sidebar = {
        .bottom_left = { 
            buffer->bounding_box.bottom_left[0], 
            buffer->bounding_box.bottom_left[1] 
        },
        .top_right = { 
            buffer->bounding_box.bottom_left[0] + s_Font.advance * 5 + 4.0f, 
            buffer->bounding_box.top_right[1] 
        }
    };

    GemQuad text_bb = {
        .bottom_left = {
            line_sidebar.top_right[0] + 10.0f,
            line_sidebar.bottom_left[1]
        },
        .top_right = {
            buffer->bounding_box.top_right[0],
            line_sidebar.top_right[1]
        }
    };
    
    // Draw sidebar with line numbers
    float penX;
    float penY = line_sidebar.top_right[1] - GEM_FONT_SIZE;

    draw_quad(&line_sidebar, NULL, (vec4){0.1f, 0.1f, 0.1f, 1.0f}, true);
    for(size_t line = buffer->camera_start_line; penY > line_sidebar.bottom_left[1]; ++line)
    {
        penX = line_sidebar.top_right[0] - s_Font.advance - 2.0f;
        if(line > buffer->contents.newline_count)
            draw_char('~', penX, penY);
        else
        {
            size_t cpy = line + 1;
            while(cpy > 0)
            {
                draw_char('0' + cpy % 10, penX, penY);
                penX -= s_Font.advance;
                cpy /= 10;
            }
        }
        penY -= (float)GEM_FONT_SIZE * 1.2f; // TODO: Make this a variable somewhere called 'line_height' or something
    }

    // Draw actual text in buffer.
    penX = text_bb.bottom_left[0];
    penY = text_bb.top_right[1];
    const PieceTree* pt = &buffer->contents;

    size_t node_offset;
    const PTNode* node = piece_tree_node_at_line(pt, buffer->camera_start_line, &node_offset);
    if(node == NULL)
        return;

    const char* buf = piece_tree_get_node_start(pt, node);
    gem_renderer_draw_str_at(buf + node_offset, node->length - node_offset, &text_bb, &penX, &penY);

    node = piece_tree_next_inorder(pt, node);
    while(node != NULL)
    {
        buf = piece_tree_get_node_start(pt, node);
        gem_renderer_draw_str_at(buf, node->length, &text_bb, &penX, &penY);
        node = piece_tree_next_inorder(pt, node);
    }

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

static void draw_quad(const GemQuad* quad, const GemQuad* tex, const vec4 color, bool is_solid)
{
    GEM_ASSERT(is_solid || tex != NULL);
    if(s_QuadCnt == MAX_QUADS)
        gem_renderer_render_batch();

    s_VertexInsert[0].position[0] = quad->bottom_left[0];
    s_VertexInsert[0].position[1] = quad->bottom_left[1];
    if(!is_solid)
    {
        s_VertexInsert[0].tex_coords[0] = tex->bottom_left[0];
        s_VertexInsert[0].tex_coords[1] = tex->bottom_left[1];
    }
    s_VertexInsert[0].color[0] = color[0];
    s_VertexInsert[0].color[1] = color[1];
    s_VertexInsert[0].color[2] = color[2];
    s_VertexInsert[0].color[3] = color[3];
    s_VertexInsert[0].solid = (float)is_solid;

    s_VertexInsert[1].position[0] = quad->top_right[0];
    s_VertexInsert[1].position[1] = quad->bottom_left[1];
    if(!is_solid)
    {
        s_VertexInsert[1].tex_coords[0] = tex->top_right[0];
        s_VertexInsert[1].tex_coords[1] = tex->bottom_left[1];
    }
    s_VertexInsert[1].color[0] = color[0];
    s_VertexInsert[1].color[1] = color[1];
    s_VertexInsert[1].color[2] = color[2];
    s_VertexInsert[1].color[3] = color[3];
    s_VertexInsert[1].solid = (float)is_solid;

    s_VertexInsert[2].position[0] = quad->top_right[0];
    s_VertexInsert[2].position[1] = quad->top_right[1];
    if(!is_solid)
    {
        s_VertexInsert[2].tex_coords[0] = tex->top_right[0];
        s_VertexInsert[2].tex_coords[1] = tex->top_right[1];
    }
    s_VertexInsert[2].color[0] = color[0];
    s_VertexInsert[2].color[1] = color[1];
    s_VertexInsert[2].color[2] = color[2];
    s_VertexInsert[2].color[3] = color[3];
    s_VertexInsert[2].solid = (float)is_solid;

    s_VertexInsert[3].position[0] = quad->bottom_left[0];
    s_VertexInsert[3].position[1] = quad->top_right[1];
    if(!is_solid)
    {
        s_VertexInsert[3].tex_coords[0] = tex->bottom_left[0];
        s_VertexInsert[3].tex_coords[1] = tex->top_right[1];
    }
    s_VertexInsert[3].color[0] = color[0];
    s_VertexInsert[3].color[1] = color[1];
    s_VertexInsert[3].color[2] = color[2];
    s_VertexInsert[3].color[3] = color[3];
    s_VertexInsert[3].solid = (float)is_solid;

    s_VertexInsert += 4;
    s_QuadCnt++;

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
