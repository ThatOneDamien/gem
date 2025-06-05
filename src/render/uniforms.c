#include "uniforms.h"
#include "core/core.h"

#include <glad/glad.h>

#include <string.h>

static GLuint s_ubo;
static struct
{
    float projection[4][4];
} s_uniforms;

void uniforms_init(void)
{
    glCreateBuffers(1, &s_ubo);
    glNamedBufferData(s_ubo, sizeof(s_uniforms), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, s_ubo);
    memset(&s_uniforms, 0, sizeof(s_uniforms));
    s_uniforms.projection[2][2] = -1.0f;
    s_uniforms.projection[3][0] = -1.0f;
    s_uniforms.projection[3][1] =  1.0f;
    s_uniforms.projection[3][3] =  1.0f;

}



void set_projection(int width, int height)
{
    GEM_ASSERT(width > 0 && height > 0);
    s_uniforms.projection[0][0] =  2.0f / (float)width;
    s_uniforms.projection[1][1] = -2.0f / (float)height;
    glNamedBufferSubData(s_ubo, 0, sizeof(s_uniforms), &s_uniforms);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
}

void uniforms_cleanup(void) { glDeleteBuffers(1, &s_ubo); }
