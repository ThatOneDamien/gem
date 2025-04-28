#include "uniforms.h"
#include "core/core.h"

#include <cglm/cglm.h>
#include <cglm/clipspace/ortho_rh_no.h>

#include <glad/glad.h>

static GLuint s_UBO;
static struct
{
    mat4 projection;
} s_Uniforms;

void gem_uniforms_init(void)
{
    glCreateBuffers(1, &s_UBO);
    glNamedBufferData(s_UBO, sizeof(s_Uniforms), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, s_UBO);
}

void gem_set_projection(int width, int height)
{
    GEM_ASSERT(width > 0 && height > 0);
    glm_ortho_rh_no(0.0f, (float)width, 0.0f, (float)height, -1.0f, 1.0f, s_Uniforms.projection);
    glNamedBufferSubData(s_UBO, 0, sizeof(s_Uniforms), &s_Uniforms);
}

void gem_uniforms_cleanup(void) { glDeleteBuffers(1, &s_UBO); }
