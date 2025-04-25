#include "core.h"
#include "gap.h"
#include "render/renderer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

static GLFWwindow* s_Window = NULL;
// static GapBuffer s_Buffer;

int main(UNUSED int argc, UNUSED char** argv)
{

    int result = glfwInit();
    GEM_ENSURE_MSG(result == GLFW_TRUE, "Failed to initialize GLFW.\n");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    s_Window = glfwCreateWindow(1080, 720, "Gem", NULL, NULL);
    glfwMakeContextCurrent(s_Window);

    int version = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    GEM_ENSURE_MSG(version != 0, "Failed to initialize OpenGL context.\n");

    gem_renderer_init();
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    while(!glfwWindowShouldClose(s_Window))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        draw_som();

        glfwSwapBuffers(s_Window);
        glfwPollEvents();
    }

    gem_renderer_cleanup();
    glfwDestroyWindow(s_Window);
    glfwTerminate();
    return 0;
}
