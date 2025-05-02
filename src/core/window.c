#include "window.h"
#include "app.h"
#include "core.h"
#include "render/uniforms.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>

#include <glad/glad.h>
#include <GL/glx.h>

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
static bool extension_supported(const char* extension);
static GLXFBConfig get_best_config(void);

/*
 * Static variables relating to the window.
 */
static Display* s_Display;
static int s_Screen;
static GLXContext s_GLContext;
static Atom s_DeleteWindowAtom;
static struct
{
    Window handle;
    int width, height;
    bool focused;
} s_Window;

void gem_window_create(uint32_t width, uint32_t height)
{
    s_Display = XOpenDisplay(NULL);
    GEM_ENSURE(s_Display != NULL);

    s_Screen = DefaultScreen(s_Display);

    int glx_major = 0;
    int glx_minor = 0;
    glXQueryVersion(s_Display, &glx_major, &glx_minor);
    GEM_ENSURE_MSG(glx_major > 1 || glx_minor >= 2, "GLX 1.2 or greater is required.");

    GLXFBConfig fbconfig = get_best_config();

    // List of events the X11 server will relay to this client.
    const long event_mask = ExposureMask |
                            KeyPressMask |
                            KeyReleaseMask |
                            ButtonPressMask |
                            ButtonReleaseMask |
                            FocusChangeMask;

    XVisualInfo* vi = glXGetVisualFromFBConfig(s_Display, fbconfig);
    XSetWindowAttributes window_attribs = {
        .border_pixel = BlackPixel(s_Display, s_Screen),
        .background_pixel = WhitePixel(s_Display, s_Screen),
        .override_redirect = True,
        .colormap = XCreateColormap(s_Display, RootWindow(s_Display, s_Screen), vi->visual, AllocNone),
        .event_mask = event_mask
    };
    const unsigned long value_mask = CWBackPixel | CWColormap | CWBorderPixel | CWEventMask;

    s_Window.handle = XCreateWindow(s_Display,                         // Display
                             RootWindow(s_Display, s_Screen), // Parent Window
                             0, 0,                              // Location
                             width, height,                     // Width and Height
                             0,                                 // Border Width
                             vi->depth,                   // Depth
                             InputOutput,                       // Class
                             vi->visual,                  // Visual
                             value_mask,                        // Value Mask
                             &window_attribs);                  // Window Attributes


    XFree(vi);
    XFreeColormap(s_Display, window_attribs.colormap);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 
        (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 6,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    if(!extension_supported("GLX_ARB_create_context") || glXCreateContextAttribsARB == NULL)
        s_GLContext = glXCreateNewContext(s_Display, fbconfig, GLX_RGBA_TYPE, 0, True);
    else
        s_GLContext = glXCreateContextAttribsARB(s_Display, fbconfig, 0, True, context_attribs);
    XSync(s_Display, False);

    glXMakeCurrent(s_Display, s_Window.handle, s_GLContext);
    XClearWindow(s_Display, s_Window.handle);
    XMapRaised(s_Display, s_Window.handle);
    Xutf8SetWMProperties(s_Display, s_Window.handle, "Gem", "Gem", NULL, 0, NULL, NULL, NULL);

    s_DeleteWindowAtom = XInternAtom(s_Display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(s_Display, s_Window.handle, &s_DeleteWindowAtom, 1);

    int version = gladLoadGLLoader((GLADloadproc)glXGetProcAddressARB);
    GEM_ENSURE_MSG(version != 0, "Failed to load OpenGL function pointers.");

    s_Window.focused = true;
    s_Window.width = width;
    s_Window.height = height;
}

void gem_window_destroy(void)
{
    glXDestroyContext(s_Display, s_GLContext);
    XDestroyWindow(s_Display, s_Window.handle);
    XCloseDisplay(s_Display);
}

void gem_window_dispatch_events(void)
{
    int prev_width = s_Window.width;
    int prev_height = s_Window.height;
    XEvent ev;
    int event_count = XPending(s_Display);
    for(int i = 0; i < event_count; ++i)
    {
        XNextEvent(s_Display, &ev);
        if(ev.type == FocusIn)
            s_Window.focused = true;
        else if(ev.type == FocusOut)
            s_Window.focused = false;
        else if(ev.type == Expose && ev.xexpose.count == 0)
        {
            s_Window.width = ev.xexpose.width;
            s_Window.height = ev.xexpose.height;
        }
        else if(ev.type == DestroyNotify || 
                (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == s_DeleteWindowAtom))
            gem_app_close();
        else
            printf("Event %d\n", ev.type);
    }

    while(!s_Window.focused)
    {
        XNextEvent(s_Display, &ev);
        if(ev.type == FocusIn)
            s_Window.focused = true;
        else if(ev.type == Expose && ev.xexpose.count == 0)
        {
            s_Window.width = ev.xexpose.width;
            s_Window.height = ev.xexpose.height;
        }
    }

    if(prev_width != s_Window.width || prev_height != s_Window.height)
        gem_set_projection(s_Window.width, s_Window.height);
}

void gem_window_swap(void)
{
    glXSwapBuffers(s_Display, s_Window.handle);
}

void gem_window_get_dims(int* width, int* height)
{
    GEM_ASSERT(width != NULL);
    GEM_ASSERT(height != NULL);
    *width = s_Window.width;
    *height = s_Window.height;
}

static bool extension_supported(const char* extension)
{
    const char* extensions = glXQueryExtensionsString(s_Display, s_Screen);
    GEM_ENSURE(extensions != NULL);

    const char* ext_loc = extension;
    for(size_t i = 0; extensions[i]; ++i)
    {
        if(extensions[i] == *ext_loc)
        {
            ++ext_loc;
            if(*ext_loc == '\0' && (extensions[i + 1] == '\0' || extensions[i + 1] == ' '))
                return true;
        }
        else
        {
            while(extensions[i] && extensions[i] != ' ')
                ++i;
            ext_loc = extension; 
        }
    }
    return false;
}

static GLXFBConfig get_best_config(void)
{
    const int glx_attribs[] = {
        GLX_X_RENDERABLE  , True,
        GLX_DRAWABLE_TYPE , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE   , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE , GLX_TRUE_COLOR,
        GLX_RED_SIZE      , 8,
        GLX_GREEN_SIZE    , 8,
        GLX_BLUE_SIZE     , 8,
        GLX_ALPHA_SIZE    , 8,
        GLX_DEPTH_SIZE    , 24,
        GLX_STENCIL_SIZE  , 8,
        GLX_DOUBLEBUFFER  , True,
        None
    };

    int count;
    GLXFBConfig* configs = glXChooseFBConfig(s_Display, s_Screen, glx_attribs, &count);
    GEM_ENSURE_MSG(configs != 0 && count > 0, "Failed to retrieve matching framebuffer configuration.");

    bool arb_multisample = extension_supported("GLX_ARB_multisample");

    GLXFBConfig best = NULL;
    int most_samples = -1;

    for(int i = 0; i < count; ++i)
    {
        GLXFBConfig cfg = configs[i];
        XVisualInfo* vi = glXGetVisualFromFBConfig(s_Display, cfg);
        if(vi == NULL)
            continue;

        int samples = 0;
        if(arb_multisample)
            glXGetFBConfigAttrib(s_Display, cfg, GLX_SAMPLES, &samples);
        if(most_samples < samples)
        {
            most_samples = samples;
            best = cfg;
        }
        XFree(vi);
    }
    XFree(configs);

    return best;
}
