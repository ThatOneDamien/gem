#include "renderer.h"
#include "font.h"
#include "uniforms.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "structs/color.h"

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

static GLuint s_vao;
static GLuint s_vbo;
static GLuint s_ibo;
static GLuint s_shader;
static GemFont s_font;
static QuadVertex* s_vertex_data;
static QuadVertex* s_vertex_insert;
static uint32_t s_quad_cnt; // Quad count of current batch
static GemRenderStats s_stats;
static bool s_initialized = false;

// Colors for rendering, this is temporary until this becomes a
// configurable setting
static vec4color s_text_color;
static vec4color s_bg_color;
static vec4color s_sidebar_color;
static vec4color s_inactive_color;
static vec4color s_cursor_color;

static void draw_fileman(const BufferWin* bufwin);
static void draw_cursor(const Cursor* cur, const View* view, vec2pos top_left);
static void handle_str(const char* str, size_t count, const GemQuad* bounding_box, const View* view, BufferPos* pos);
static void draw_char(char c, vec2pos pos, vec4color color);
static void draw_quad(const GemQuad* quad, const float tex[4], vec4color color, bool is_solid);
static bool create_shader_program(const char* vert_path, const char* frag_path, GLuint* program_id);

#ifdef GEM_DEBUG
static APIENTRY void debugCallbackFunc(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*,
                                       const void*);
#endif // GEM_DEBUG

void renderer_init(void)
{
    GEM_ASSERT(!s_initialized);
#ifdef GEM_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debugCallbackFunc, NULL);
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glCreateVertexArrays(1, &s_vao);
    glCreateBuffers(1, &s_vbo);
    glCreateBuffers(1, &s_ibo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertex) * MAX_VERTICES, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ibo);

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

    bool result = create_shader_program(TEXT_VERT_SHADER, TEXT_FRAG_SHADER, &s_shader);
    GEM_ENSURE_MSG(result, "Failed to create text shader, exiting.");
    glUseProgram(s_shader);

    s_vertex_data = malloc(MAX_VERTICES * sizeof(QuadVertex));
    s_vertex_insert = s_vertex_data;

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

    result = gen_font_atlas(DEFAULT_FONT, &s_font);
    GEM_ENSURE_MSG(result, "Failed to create font atlas.");
    glBindTextureUnit(0, s_font.atlas_texture);

    uniforms_init();

    s_text_color     = hex_rgba_to_color(0xd2d2dfff);
    s_bg_color       = hex_rgba_to_color(0x2d2d3dff);
    s_sidebar_color  = hex_rgba_to_color(0x36364aff);
    s_inactive_color = hex_rgba_to_color(0x00000044); 
    s_cursor_color   = hex_rgba_to_color(0xaaaaaaa0);

    GEM_ENSURE_ARGS(result, "Failed to create font at path %s.", DEFAULT_FONT);
    s_initialized = true;
}

void renderer_cleanup(void)
{
    if(s_initialized)
    {
        s_initialized = false;
        free(s_vertex_data);
        uniforms_cleanup();
        glDeleteProgram(s_shader);
        glDeleteTextures(1, &s_font.atlas_texture);
        glDeleteBuffers(1, &s_vbo);
        glDeleteBuffers(1, &s_ibo);
        glDeleteVertexArrays(1, &s_vao);
    }
}

void renderer_start_batch(void)
{
    s_vertex_insert = s_vertex_data;
    s_quad_cnt = 0;
    s_stats.draw_calls = 0;
    s_stats.quad_count = 0;
}

void renderer_render_batch(void)
{
    if(s_quad_cnt == 0)
        return;

    glBufferSubData(GL_ARRAY_BUFFER, 0, (uintptr_t)s_vertex_insert - (uintptr_t)s_vertex_data, s_vertex_data);
    glDrawElements(GL_TRIANGLES, s_quad_cnt * 6, GL_UNSIGNED_INT, NULL);

    s_stats.quad_count += s_quad_cnt; // Add quad count of current batch to the total
    s_stats.draw_calls++;
    s_vertex_insert = s_vertex_data;
    s_quad_cnt = 0;
}

static const GemGlyphData* get_glyph_data_from_font(const GemFont* font, char c)
{
    size_t index = c >= GEM_PRINTABLE_ASCII_START && c < GEM_PRINTABLE_ASCII_END
                       ? (size_t)c - GEM_PRINTABLE_ASCII_START + 1
                       : 0;
    return font->glyphs[index].width == 0 ? &font->glyphs[0] : &font->glyphs[index];
}



void renderer_draw_bufwin(const BufferWin* bufwin, bool active)
{
    GEM_ASSERT(bufwin != NULL);

    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    const GemQuad* buf_bb = &bufwin->frame.bounding_box;
    
    // Draw background
    draw_quad(buf_bb, NULL, s_bg_color, true);

    if(bufwin->mode == WIN_MODE_FILEMAN)
    {
        draw_fileman(bufwin);
    }
    else
    {
        uint32_t num_pad = s_font.advance / 4;

        // Draw sidebar with line numbers
        vec2pos   pen;
        pen.y = bufwin->line_num_bb.tr.y + bufwin->text_padding.top;

        float vert_advance = get_vert_advance();

        draw_quad(&bufwin->line_num_bb, NULL, s_sidebar_color, true);
        for(int64_t line = 0; line < bufwin->view.count.line; ++line)
        {
            size_t cpy = line + 1 + bufwin->view.start.line;
            pen.x = bufwin->line_num_bb.tr.x - s_font.advance - num_pad;
            if(cpy > pt->line_cnt)
                draw_char('~', pen, s_text_color);
            else
            {
                while(cpy > 0)
                {
                    draw_char('0' + cpy % 10, pen, s_text_color);
                    pen.x -= s_font.advance;
                    cpy /= 10;
                }
            }
            pen.y += vert_advance;
        }

        // Draw actual text in buffer.
        pen.x = bufwin->contents_bb.bl.x;
        pen.y = bufwin->contents_bb.tr.y;

        size_t node_offset;
        BufferPos view_pos = { 0, -bufwin->view.start.column };
        const char* buf;
        const PTNode* node = piece_tree_node_at_line(pt, bufwin->view.start.line, &node_offset);

        while(node != NULL)
        {
            buf = piece_tree_get_node_start(pt, node) + node_offset;
            handle_str(buf, node->length - node_offset, &bufwin->contents_bb, &bufwin->view, &view_pos);
            node_offset = 0;
            node = piece_tree_next_inorder(pt, node);
        }

        pen.x = bufwin->contents_bb.bl.x;
        pen.y = bufwin->contents_bb.tr.y;
        draw_cursor(&bufwin->cursor, &bufwin->view, pen); 
    }
    if(!active)
        draw_quad(buf_bb, NULL, s_inactive_color, true);
}

const GemFont* gem_get_font(void)
{
    return &s_font;
}

const GemRenderStats* renderer_get_stats(void)
{
    return &s_stats;
}

static void draw_fileman(const BufferWin* bufwin)
{
    if(bufwin->dir_entries.size == 0)
        return;

    const GemQuad* frame_bb = &bufwin->frame.bounding_box;
    GemQuad bb = make_quad(frame_bb->bl.x + s_font.advance * 2,
                           frame_bb->bl.y - bufwin->text_padding.bottom,
                           frame_bb->tr.x - s_font.advance * 2,
                           frame_bb->tr.y + bufwin->text_padding.top);

    BufferPos pos = { 0, 0 };
    uint32_t vert_adv = get_vert_advance();
    uint32_t hori_adv = s_font.advance;

    for(size_t i = 0; i < bufwin->dir_entries.size; ++i)
    {
        DirEntry* e = bufwin->dir_entries.data + i;
        size_t len = strlen(e->name);
        handle_str(e->name, len, &bb, &bufwin->view, &pos);
        if(i == bufwin->sel_entry)
        {
            GemQuad q = make_quad(bb.bl.x,
                                  bb.tr.y + (i + 1) * vert_adv,
                                  bb.bl.x + len * hori_adv,
                                  bb.tr.y + i * vert_adv);
            draw_quad(&q, NULL, s_cursor_color, true);
        }
        // pos.column = bufwin->dir_entries.largest_name + 2;
        // switch(bufwin->dir_entries.ents[i]->d_type)
        // {
        // case 8:
        //     handle_str("FILE", 4, &bb, &bufwin->view, &pos);
        //     break;
        // default:
        //     handle_str("DIR", 3, &bb, &bufwin->view, &pos);
        //     break;
        // }
        pos.line++;
        pos.column = 0;
    }
}

static void draw_cursor(const Cursor* cur, const View* view, vec2pos top_left)
{
    int64_t rel_line = cur->vis.line - view->start.line;
    int64_t rel_col = cur->vis.column - view->start.column;
    if(rel_line < 0 || rel_line >= view->count.line ||
       rel_col < 0 || rel_col >= view->count.column)
        return;


    int vert_adv = get_vert_advance();
    int hori_adv = s_font.advance;
    int x = top_left.x + rel_col * hori_adv;
    int y = top_left.y + rel_line * vert_adv;
    GemQuad cursor_quad = make_quad(x, 
                                    y + vert_adv,
                                    x + hori_adv, 
                                    y);
    draw_quad(&cursor_quad, NULL, s_cursor_color, true);
}



static void handle_str(const char* str, size_t count, const GemQuad* bounding_box, 
                       const View* view, BufferPos* pos)
{
    int vert_adv = get_vert_advance();
    int hori_adv = s_font.advance;
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
                draw_char(c, loc, s_text_color); 
            }
            pos->column++;
        }
    }
}

static void draw_char(char c, vec2pos pos, vec4color color)
{
    const GemGlyphData* data = get_glyph_data_from_font(&s_font, c);
    pos.x += data->xoff;
    pos.y += get_font_size() - data->yoff;
    GemQuad char_quad = make_quad(pos.x,
                                  pos.y + data->height,
                                  pos.x + data->width, 
                                  pos.y);

    draw_quad(&char_quad, data->tex_coords, color, false);
}

static void draw_quad(const GemQuad* quad, const float tex[4], vec4color color, bool is_solid)
{
    GEM_ASSERT(quad != NULL);
    GEM_ASSERT(is_solid || tex != NULL);
    if(s_quad_cnt == MAX_QUADS)
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
        s_vertex_insert[i].position[0] = positions[2 * i];
        s_vertex_insert[i].position[1] = positions[2 * i + 1];
        if(!is_solid)
        {
            s_vertex_insert[i].tex_coords[0] = texc[2 * i];
            s_vertex_insert[i].tex_coords[1] = texc[2 * i + 1];
        }
        s_vertex_insert[i].color.r = color.r;
        s_vertex_insert[i].color.g = color.g;
        s_vertex_insert[i].color.b = color.b;
        s_vertex_insert[i].color.a = color.a;
        s_vertex_insert[i].solid = (float)is_solid;
    }

    s_vertex_insert += 4;
    s_quad_cnt++;
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
