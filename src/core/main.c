#include "core.h"
#include "gap.h"
#include "render/font.h"
#include "render/renderer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

static GLFWwindow* s_Window = NULL;
// static GapBuffer s_Buffer;

int main(UNUSED int argc, UNUSED char** argv)
{

    int result = glfwInit();
    GEM_ENSURE_MSG(result == GLFW_TRUE, "Failed to initialize GLFW.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    s_Window = glfwCreateWindow(1080, 720, "Gem", NULL, NULL);
    glfwMakeContextCurrent(s_Window);

    int version = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    GEM_ENSURE_MSG(version != 0, "Failed to initialize OpenGL context.");

    gem_freetype_init();
    gem_renderer_init();
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    while(!glfwWindowShouldClose(s_Window))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        char c[GEM_GLYPH_CNT - 1];
        for(size_t i = 0; i < GEM_GLYPH_CNT - 1; ++i)
            c[i] = (char)(i + GEM_PRINTABLE_ASCII_START);
        draw_som(c);

        glfwSwapBuffers(s_Window);
        glfwPollEvents();
    }

    gem_renderer_cleanup();
    gem_freetype_cleanup();
    glfwDestroyWindow(s_Window);
    glfwTerminate();
    return 0;
}
