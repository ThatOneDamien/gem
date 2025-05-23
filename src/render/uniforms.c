#include "uniforms.h"
#include "core/core.h"

#include <glad/glad.h>

#include <string.h>

static GLuint s_UBO;
static struct
{
    float projection[4][4];
} s_Uniforms;

void uniforms_init(void)
{
    glCreateBuffers(1, &s_UBO);
    glNamedBufferData(s_UBO, sizeof(s_Uniforms), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, s_UBO);
    memset(&s_Uniforms, 0, sizeof(s_Uniforms));
    s_Uniforms.projection[2][2] = -1.0f;
    s_Uniforms.projection[3][0] = -1.0f;
    s_Uniforms.projection[3][1] =  1.0f;
    s_Uniforms.projection[3][3] =  1.0f;

}



void set_projection(int width, int height)
{
    GEM_ASSERT(width > 0 && height > 0);
    s_Uniforms.projection[0][0] =  2.0f / (float)width;
    s_Uniforms.projection[1][1] = -2.0f / (float)height;
    glNamedBufferSubData(s_UBO, 0, sizeof(s_Uniforms), &s_Uniforms);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
}

void uniforms_cleanup(void) { glDeleteBuffers(1, &s_UBO); }
