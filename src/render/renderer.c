#include "renderer.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "font.h"
#include "uniforms.h"

#include <glad/glad.h>

#include <string.h>

#define MAX_QUADS    2000
#define MAX_VERTICES (MAX_QUADS * 4)
#define INDEX_COUNT  (MAX_QUADS * 6)

#define TEXT_VERT_SHADER "assets/shaders/basic.vert"
#define TEXT_FRAG_SHADER "assets/shaders/basic.frag"
#define DEFAULT_FONT     "assets/fonts/JetBrainsMono-SemiBold.ttf"

typedef struct
{
    vec2pos   position;
    vec2pos   tex_coords;
    vec4color color;
    float     solid;
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
static void draw_quad(const GemQuad* quad, const GemQuad* tex, vec4color color, bool is_solid);
static void draw_char(char c, vec2pos pos, vec4color color);
static void handle_char(char c, const GemQuad* bounding_box, vec2pos* pen);

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
    vec2pos pen;
    pen.x = bounding_box->bl.x;
    pen.y = bounding_box->tr.y;
    gem_renderer_draw_str_at(str, strlen(str), bounding_box, &pen);
}

static const GemGlyphData* get_glyph_data_from_font(const GemFont* font, char c)
{
    size_t index = c >= GEM_PRINTABLE_ASCII_START && c < GEM_PRINTABLE_ASCII_END
                       ? (size_t)c - GEM_PRINTABLE_ASCII_START + 1
                       : 0;
    return font->glyphs[index].width == 0 ? &font->glyphs[0] : &font->glyphs[index];
}

static void draw_char(char c, vec2pos pos, vec4color color)
{
    const GemGlyphData* data = get_glyph_data_from_font(&s_Font, c);
    pos.x += data->xoff;
    pos.y += gem_get_font_size() - data->yoff;
    GemQuad char_quad = make_quad(pos.x,
                                  pos.y + data->height,
                                  pos.x + data->width, 
                                  pos.y);

    draw_quad(&char_quad, &data->tex_coords, color, false);
}

static void handle_char(char c, const GemQuad* bounding_box, vec2pos* pen)
{
    if(c == '\n')
    {
        pen->x = bounding_box->bl.x;
        pen->y += gem_get_vert_advance();
    }
    else if(c == ' ')
        pen->x += s_Font.advance;
    else if(c == '\t')
        pen->x += s_Font.advance * 4; // TODO: Change this to actually be correct lmao.
    else if(pen->x < bounding_box->tr.x)
    {
        draw_char(c, *pen, (vec4color){ 0.7f, 0.7f, 0.7f, 1.0f });
        pen->x += s_Font.advance;
    }
}

void gem_renderer_draw_str_at(const char* str, size_t count, const GemQuad* bounding_box, vec2pos* pen)
{
    for(size_t i = 0; i < count && pen->y < bounding_box->bl.y; ++i)
        handle_char(str[i], bounding_box, pen);
}

static void draw_cursor(const Cursor* cur, vec2pos pos)
{
    (void)cur;
    GemQuad cursor_quad = make_quad(pos.x, pos.y + gem_get_vert_advance(),
                                    pos.x + s_Font.advance, pos.y);
    draw_quad(&cursor_quad, NULL, (vec4color){1.0f, 1.0f, 1.0f, 0.4f}, true);

}

void gem_renderer_draw_buffer(const TextBuffer* buffer)
{
    GEM_ASSERT(buffer != NULL);

    const GemQuad* buf_bb = &buffer->bounding_box;
    GemQuad line_sidebar = make_quad(buf_bb->bl.x,
                                     buf_bb->bl.y,
                                     buf_bb->bl.x + s_Font.advance * 5 + 4.0f,
                                     buf_bb->tr.y);
    
    GemQuad text_bb = make_quad(line_sidebar.tr.x + 10.0f,
                                buf_bb->bl.y,
                                buf_bb->tr.x,
                                buf_bb->tr.y);

    // Draw sidebar with line numbers
    vec2pos   pen;
    vec4color text_color = { 0.7f, 0.7f, 0.7f, 1.0f };
    vec4color sidebar_color = { 0.1f, 0.1f, 0.1f, 1.0f };
    pen.y = line_sidebar.tr.y;


    draw_quad(&line_sidebar, NULL, sidebar_color, true);
    for(size_t line = buffer->camera_start_line; pen.y < line_sidebar.bl.y; ++line)
    {
        pen.x = line_sidebar.tr.x - s_Font.advance - 2.0f;
        if(line >= buffer->contents.line_cnt)
            draw_char('~', pen, text_color);
        else
        {
            size_t cpy = line + 1;
            while(cpy > 0)
            {
                draw_char('0' + cpy % 10, pen, text_color);
                pen.x -= s_Font.advance;
                cpy /= 10;
            }
        }
        pen.y += gem_get_vert_advance(); // TODO: Make this a variable somewhere called 'line_height' or something
    }

    // Draw actual text in buffer.
    pen.x = text_bb.bl.x;
    pen.y = text_bb.tr.y;

    size_t node_offset;
    const char* buf;
    const PieceTree* pt = &buffer->contents;
    size_t start_offset = piece_tree_get_offset(pt, buffer->camera_start_line, 0);
    size_t cur_offset = start_offset;
    vec2pos cursor_pen = pen;
    const PTNode* node = piece_tree_node_at_line(pt, buffer->camera_start_line, &node_offset);

    if(node == NULL)
        goto drawcursor;

    buf = piece_tree_get_node_start(pt, node) + node_offset;
    node_offset = node->length - node_offset;
    for(size_t i = 0; i < node_offset && pen.y < text_bb.bl.y; ++i)
    {
        handle_char(buf[i], &text_bb, &pen);
        cur_offset++;
        if(cur_offset == buffer->cursor.offset)
            cursor_pen = pen;
    }

    node = piece_tree_next_inorder(pt, node);
    while(node != NULL)
    {
        buf = piece_tree_get_node_start(pt, node);
        for(size_t i = 0; i < node->length && pen.y < text_bb.bl.y; ++i)
        {
            handle_char(buf[i], &text_bb, &pen);
            cur_offset++;
            if(cur_offset == buffer->cursor.offset)
                cursor_pen = pen;
        }
        node = piece_tree_next_inorder(pt, node);
    }
drawcursor:
    if(buffer->cursor.offset >= start_offset && 
       buffer->cursor.offset <= cur_offset)
        draw_cursor(&buffer->cursor, cursor_pen);
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

static void draw_quad(const GemQuad* quad, const GemQuad* tex, vec4color color, bool is_solid)
{
    GEM_ASSERT(is_solid || tex != NULL);
    if(s_QuadCnt == MAX_QUADS)
        gem_renderer_render_batch();

    s_VertexInsert[0].position.x = quad->bl.x;
    s_VertexInsert[0].position.y = quad->bl.y;
    if(!is_solid)
    {
        s_VertexInsert[0].tex_coords.x = tex->bl.x;
        s_VertexInsert[0].tex_coords.y = tex->bl.y;
    }
    s_VertexInsert[0].color.r = color.r;
    s_VertexInsert[0].color.g = color.g;
    s_VertexInsert[0].color.b = color.b;
    s_VertexInsert[0].color.a = color.a;
    s_VertexInsert[0].solid = (float)is_solid;

    s_VertexInsert[1].position.x = quad->tr.x;
    s_VertexInsert[1].position.y = quad->bl.y;
    if(!is_solid)
    {
        s_VertexInsert[1].tex_coords.x = tex->tr.x;
        s_VertexInsert[1].tex_coords.y = tex->bl.y;
    }
    s_VertexInsert[1].color.r = color.r;
    s_VertexInsert[1].color.g = color.g;
    s_VertexInsert[1].color.b = color.b;
    s_VertexInsert[1].color.a = color.a;
    s_VertexInsert[1].solid = (float)is_solid;

    s_VertexInsert[2].position.x = quad->tr.x;
    s_VertexInsert[2].position.y = quad->tr.y;
    if(!is_solid)
    {
        s_VertexInsert[2].tex_coords.x = tex->tr.x;
        s_VertexInsert[2].tex_coords.y = tex->tr.y;
    }
    s_VertexInsert[2].color.r = color.r;
    s_VertexInsert[2].color.g = color.g;
    s_VertexInsert[2].color.b = color.b;
    s_VertexInsert[2].color.a = color.a;
    s_VertexInsert[2].solid = (float)is_solid;

    s_VertexInsert[3].position.x = quad->bl.x;
    s_VertexInsert[3].position.y = quad->tr.y;
    if(!is_solid)
    {
        s_VertexInsert[3].tex_coords.x = tex->bl.x;
        s_VertexInsert[3].tex_coords.y = tex->tr.y;
    }
    s_VertexInsert[3].color.r = color.r;
    s_VertexInsert[3].color.g = color.g;
    s_VertexInsert[3].color.b = color.b;
    s_VertexInsert[3].color.a = color.a;
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
