#include "core.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

static GLFWwindow* s_Window = NULL;

int main(UNUSED int argc, UNUSED char** argv)
{

    int result = glfwInit();
    GEM_ENSURE_MSG(result == GLFW_TRUE, "Failed to initialize GLFW.\n");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    s_Window = glfwCreateWindow(1080, 720, "Gem", NULL, NULL);
    glfwMakeContextCurrent(s_Window);

    int version = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    GEM_ENSURE_MSG(version != 0, "Failed to initialize OpenGL context.\n");

    while(!glfwWindowShouldClose(s_Window))
    {
        
        glfwSwapBuffers(s_Window);
        glfwPollEvents();
    }
 
    glfwDestroyWindow(s_Window);
    glfwTerminate();
    return 0;
}
