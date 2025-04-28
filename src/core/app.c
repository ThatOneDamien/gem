#include "app.h"
#include "core.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "timing.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#include <time.h>

#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static GLFWwindow* s_Window = NULL;
static bool s_Running = false;

#ifdef GEM_DEBUG
static void glfw_err(int, const char*);
#endif

static void create_window(void);
static void set_window_callbacks(void);

void gem_app_init(UNUSED int argc, UNUSED char* argv[])
{
    int result = glfwInit();
    GEM_ENSURE_MSG(result == GLFW_TRUE, "Failed to initialize GLFW.");

#ifdef GEM_DEBUG
    glfwSetErrorCallback(glfw_err);
#endif

    create_window();
    gem_freetype_init();
    gem_renderer_init();
}

void gem_app_run(void)
{
    s_Running = true;
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    int prev_width = 0, prev_height = 0;

    glfwSwapBuffers(s_Window);
    while(s_Running)
    {
        glfwPollEvents();
        // Don't waste CPU resources if not focused
        if(glfwGetWindowAttrib(s_Window, GLFW_FOCUSED) == GLFW_TRUE)
        {
            int new_width, new_height;
            glfwGetFramebufferSize(s_Window, &new_width, &new_height);
            if(new_width != prev_width || new_height != prev_height)
            {
                prev_width = new_width;
                prev_height = new_height;
                glViewport(0, 0, new_width, new_height);
                gem_set_projection(new_width, new_height);
            }


            glClear(GL_COLOR_BUFFER_BIT);
            char str[GEM_GLYPH_CNT - 1];
            for(size_t i = 0; i < GEM_GLYPH_CNT - 1; ++i)
                str[i] = (char)(i + GEM_PRINTABLE_ASCII_START);
            GemQuad bounding_box = {
                .bottom_left = {             0.0f,              0.0f },
                .top_right = { (float)new_width, (float)new_height }
            };
            gem_draw_str(str, &bounding_box);

            glfwSwapBuffers(s_Window);
        }
    }

    gem_renderer_cleanup();
    gem_freetype_cleanup();
    glfwDestroyWindow(s_Window);
    glfwTerminate();
}

void gem_app_close(void) { s_Running = false; }

static void create_window(void)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    s_Window = glfwCreateWindow(GEM_INITIAL_WIDTH, GEM_INITIAL_HEIGHT, "Gem", NULL, NULL);
    glfwMakeContextCurrent(s_Window);

    set_window_callbacks();

    int version = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    GEM_ENSURE_MSG(version != 0, "Failed to initialize OpenGL context.");
}

#ifdef GEM_DEBUG
static void glfw_err(int error_code, const char* description)
{
    GEM_ERROR_ARGS("GLFW Error (code %d): %s", error_code, description);
}
#endif

static void mouse_button_callback(UNUSED GLFWwindow* window, UNUSED int button, UNUSED int action,
                                  UNUSED int mods)
{
}

static void mouse_scroll_callback(UNUSED GLFWwindow* window, UNUSED double xoffset,
                                  UNUSED double yoffset)
{
}

static void key_callback(UNUSED GLFWwindow* window, UNUSED int key, UNUSED int scancode,
                         UNUSED int action, UNUSED int mods)
{
}

static void set_window_callbacks(void)
{
    glfwSetWindowCloseCallback(s_Window, (GLFWwindowclosefun)gem_app_close);
    glfwSetMouseButtonCallback(s_Window, mouse_button_callback);
    glfwSetScrollCallback(s_Window, mouse_scroll_callback);
    glfwSetKeyCallback(s_Window, key_callback);
}
