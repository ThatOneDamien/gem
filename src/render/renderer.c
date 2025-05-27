#include "renderer.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "font.h"
#include "uniforms.h"

#include <glad/glad.h>

#include <math.h>
#include <string.h>

#define MAX_QUADS    2000
#define MAX_VERTICES (MAX_QUADS * 4)
#define INDEX_COUNT  (MAX_QUADS * 6)

#define TEXT_VERT_SHADER "assets/shaders/basic.vert"
#define TEXT_FRAG_SHADER "assets/shaders/basic.frag"
#define DEFAULT_FONT     "assets/fonts/JetBrainsMono-Regular.ttf"

typedef struct QuadVertex QuadVertex;
struct QuadVertex
{
    float     position[2];
    float     tex_coords[2];
    vec4color color;
    float     solid;
};

static GLuint s_VAO;
static GLuint s_VBO;
static GLuint s_IBO;
static GLuint s_Shader;
static GemFont s_Font;
static QuadVertex* s_VertexData;
static QuadVertex* s_VertexInsert;
static uint32_t s_QuadCnt; // Quad count of current batch
static GemRenderStats s_Stats;
static bool s_Initialized = false;

static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id);
static void draw_quad(const GemQuad* quad, const float tex[4], vec4color color, bool is_solid);
static void draw_char(char c, vec2pos pos, vec4color color);
static void handle_str(const char* str, size_t count, const GemQuad* bounding_box, const View* view, BufferPos* pos);

#ifdef GEM_DEBUG
static APIENTRY void debugCallbackFunc(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*,
                                       const void*);
#endif // GEM_DEBUG

void renderer_init(void)
{
    GEM_ASSERT(!s_Initialized);
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

    s_VertexData = malloc(MAX_VERTICES * sizeof(QuadVertex));
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

    result = gen_font_atlas(DEFAULT_FONT, &s_Font);
    GEM_ENSURE_MSG(result, "Failed to create font atlas.");
    glBindTextureUnit(0, s_Font.atlas_texture);

    uniforms_init();

    GEM_ENSURE_ARGS(result, "Failed to create font at path %s.", DEFAULT_FONT);
    s_Initialized = true;
}

void renderer_cleanup(void)
{
    if(s_Initialized)
    {
        s_Initialized = false;
        free(s_VertexData);
        uniforms_cleanup();
        glDeleteProgram(s_Shader);
        glDeleteTextures(1, &s_Font.atlas_texture);
        glDeleteBuffers(1, &s_VBO);
        glDeleteBuffers(1, &s_IBO);
        glDeleteVertexArrays(1, &s_VAO);
    }
}

void renderer_start_batch(void)
{
    s_VertexInsert = s_VertexData;
    s_QuadCnt = 0;
    s_Stats.draw_calls = 0;
    s_Stats.quad_count = 0;
}

void renderer_render_batch(void)
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

static void draw_char(char c, vec2pos pos, vec4color color)
{
    const GemGlyphData* data = get_glyph_data_from_font(&s_Font, c);
    pos.x += data->xoff;
    pos.y += get_font_size() - data->yoff;
    GemQuad char_quad = make_quad(pos.x,
                                  pos.y + data->height,
                                  pos.x + data->width, 
                                  pos.y);

    draw_quad(&char_quad, data->tex_coords, color, false);
}

static void UNUSED handle_str(const char* str, size_t count, const GemQuad* bounding_box, 
                       const View* view, BufferPos* pos)
{
    int vert_adv = get_vert_advance();
    int hori_adv = s_Font.advance;
    for(size_t i = 0; i < count && pos->line < view->count.line; ++i)
    {
        char c = str[i];
        if(c == '\n')
        {
            pos->line++;
            pos->column = -view->start.column;
        }
        else if(c == ' ')
            pos->column++;
        else if(c == '\t')
            pos->column += 4 - (pos->column + view->start.column) % 4;
        else
        {
            if(pos->column >= 0 && pos->column < view->count.column)
            {
                vec2pos loc = { 
                    bounding_box->bl.x + pos->column * hori_adv,
                    bounding_box->tr.y + pos->line * vert_adv
                };
                draw_char(c, loc, (vec4color){ 0.7f, 0.7f, 0.7f, 1.0f });
            }
            pos->column++;
        }
    }
}

static void UNUSED draw_cursor(const Cursor* cur, const View* view, vec2pos top_left)
{
    int64_t rel_line = cur->vis.line - view->start.line;
    int64_t rel_col = cur->vis.column - view->start.column;
    if(rel_line < 0 || rel_line >= view->count.line ||
       rel_col < 0 || rel_col >= view->count.column)
        return;


    int vert_adv = get_vert_advance();
    int hori_adv = s_Font.advance;
    int x = top_left.x + rel_col * hori_adv;
    int y = top_left.y + rel_line * vert_adv;
    GemQuad cursor_quad = make_quad(x, 
                                    y + vert_adv,
                                    x + hori_adv, 
                                    y);
    draw_quad(&cursor_quad, NULL, (vec4color){1.0f, 1.0f, 1.0f, 0.4f}, true);

}

void renderer_draw_bufwin(const BufferWin* bufwin, bool active)
{
    GEM_ASSERT(bufwin != NULL);

    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    const GemQuad* buf_bb = bufwin_get_bb(bufwin);
    
    // Draw background
    draw_quad(buf_bb, NULL, (vec4color){0.02f, 0.03f, 0.05f, 1.0f}, true);

    uint32_t num_pad = s_Font.advance / 4;
    uint32_t line_num_width = pt->line_cnt < 1000 ? 4 : (log10((double)pt->line_cnt) + 2);
    GemQuad line_sidebar = make_quad(buf_bb->bl.x,
                                     buf_bb->bl.y + 1,
                                     buf_bb->bl.x + s_Font.advance * line_num_width + num_pad * 2,
                                     buf_bb->tr.y);

    GemQuad text_bb = make_quad(line_sidebar.tr.x + bufwin->text_padding.left,
                                buf_bb->bl.y - bufwin->text_padding.bottom,
                                buf_bb->tr.x - bufwin->text_padding.right,
                                buf_bb->tr.y + bufwin->text_padding.top);

    // Draw sidebar with line numbers
    vec2pos   pen;
    vec4color text_color = { 0.7f, 0.7f, 0.7f, 1.0f };
    vec4color sidebar_color = { 0.1f, 0.1f, 0.1f, 1.0f };
    pen.y = line_sidebar.tr.y + bufwin->text_padding.top;

    float vert_advance = get_vert_advance();

    draw_quad(&line_sidebar, NULL, sidebar_color, true);
    for(int64_t line = 0; line < bufwin->view.count.line; ++line)
    {
        size_t cpy = line + 1 + bufwin->view.start.line;
        pen.x = line_sidebar.tr.x - s_Font.advance - num_pad;
        if(cpy > pt->line_cnt)
            draw_char('~', pen, text_color);
        else
        {
            while(cpy > 0)
            {
                draw_char('0' + cpy % 10, pen, text_color);
                pen.x -= s_Font.advance;
                cpy /= 10;
            }
        }
        pen.y += vert_advance;
    }

    // Draw actual text in buffer.
    pen.x = text_bb.bl.x;
    pen.y = text_bb.tr.y;

    size_t node_offset;
    BufferPos view_pos = { 0, -bufwin->view.start.column };
    const char* buf;
    const PTNode* node = piece_tree_node_at_line(pt, bufwin->view.start.line, &node_offset);

    while(node != NULL)
    {
        buf = piece_tree_get_node_start(pt, node) + node_offset;
        handle_str(buf, node->length - node_offset, &text_bb, &bufwin->view, &view_pos);
        node_offset = 0;
        node = piece_tree_next_inorder(pt, node);
    }

    pen.x = text_bb.bl.x;
    pen.y = text_bb.tr.y;
    draw_cursor(&bufwin->cursor, &bufwin->view, pen); 
    if(!active)
        draw_quad(buf_bb, NULL, (vec4color){0.0f, 0.0f, 0.0f, 0.4f}, true);
}

const GemFont* gem_get_font(void)
{
    return &s_Font;
}

const GemRenderStats* renderer_get_stats(void)
{
    return &s_Stats;
}

static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id)
{
    GLuint program = 0;
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    char* src;
    size_t src_size;
    read_entire_file(vert_path, &src, &src_size, NULL);

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


    read_entire_file(frag_path, &src, &src_size, NULL);
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

static void draw_quad(const GemQuad* quad, const float tex[4], vec4color color, bool is_solid)
{
    GEM_ASSERT(quad != NULL);
    GEM_ASSERT(is_solid || tex != NULL);
    if(s_QuadCnt == MAX_QUADS)
        renderer_render_batch();

    float positions[8] = { 
        quad->bl.x, quad->bl.y,
        quad->tr.x, quad->bl.y,
        quad->tr.x, quad->tr.y,
        quad->bl.x, quad->tr.y
    };

    float texc[8];
    if(!is_solid)
    {
        texc[0] = tex[0];
        texc[1] = tex[1];
        texc[2] = tex[2];
        texc[3] = tex[1];
        texc[4] = tex[2];
        texc[5] = tex[3];
        texc[6] = tex[0];
        texc[7] = tex[3];
    }

    for(size_t i = 0; i < 4; ++i)
    {
        s_VertexInsert[i].position[0] = positions[2 * i];
        s_VertexInsert[i].position[1] = positions[2 * i + 1];
        if(!is_solid)
        {
            s_VertexInsert[i].tex_coords[0] = texc[2 * i];
            s_VertexInsert[i].tex_coords[1] = texc[2 * i + 1];
        }
        s_VertexInsert[i].color.r = color.r;
        s_VertexInsert[i].color.g = color.g;
        s_VertexInsert[i].color.b = color.b;
        s_VertexInsert[i].color.a = color.a;
        s_VertexInsert[i].solid = (float)is_solid;
    }

    s_VertexInsert += 4;
    s_QuadCnt++;
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
