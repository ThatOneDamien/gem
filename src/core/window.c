#include "window.h"
#include "app.h"
#include "core.h"
#include "keycode.h"
#include "render/uniforms.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <glad/glad.h>
#include <GL/glx.h>

#include <string.h>

#define REPEAT_INTERVAL 300

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef struct MouseState MouseState;
typedef struct GemWindow  GemWindow;

struct MouseState
{
    uint32_t mouse_code;
    Time     last_time;
    int      count_in_seq;
};

struct GemWindow
{
    Window handle;
    int    width;
    int    height;
    bool   focused;
};

static bool extension_supported(const char* extension);
static GLXFBConfig get_best_config(void);
static void set_keymap(void);
static int translate_keysym(const KeySym* keysyms, int width);

extern void bufwin_update_screen(int width, int height);

/*
 * Static variables relating to the window.
 */
static Display*   s_display;
static int        s_screen;
static GLXContext s_gl_context;
static GemWindow  s_window;

static Atom       s_del_win_atom;
static Atom       s_state_atom;
static Atom       s_fullscreen_atom;
static Atom       s_maximizeh_atom;
static Atom       s_maximizev_atom;

static MouseState s_last_mouse;
static int        s_keymap[256];


static bool s_Initialized = false;

void window_create(uint32_t width, uint32_t height)
{
    GEM_ASSERT(!s_Initialized);
    s_display = XOpenDisplay(NULL);
    GEM_ENSURE(s_display != NULL);

    s_screen = DefaultScreen(s_display);

    int glx_major = 0;
    int glx_minor = 0;
    glXQueryVersion(s_display, &glx_major, &glx_minor);
    GEM_ENSURE_MSG(glx_major > 1 || glx_minor >= 2, "GLX 1.2 or greater is required.");

    GLXFBConfig fbconfig = get_best_config();

    // List of events the X11 server will relay to this client.
    const long event_mask = ExposureMask |
                            KeyPressMask |
                            ButtonPressMask |
                            ButtonReleaseMask |
                            FocusChangeMask;

    XVisualInfo* vi = glXGetVisualFromFBConfig(s_display, fbconfig);
    XSetWindowAttributes window_attribs = {
        .border_pixel = BlackPixel(s_display, s_screen),
        .background_pixel = BlackPixel(s_display, s_screen),
        .override_redirect = True,
        .colormap = XCreateColormap(s_display, RootWindow(s_display, s_screen), vi->visual, AllocNone),
        .event_mask = event_mask
    };
    const unsigned long value_mask = CWBackPixel | CWColormap | CWBorderPixel | CWEventMask;

    s_window.handle = XCreateWindow(s_display,                          // Display
                                    RootWindow(s_display, s_screen),    // Parent Window
                                    0, 0,                               // Location
                                    width, height,                      // Width and Height
                                    1,                                  // Border Width
                                    vi->depth,                          // Depth
                                    InputOutput,                        // Class
                                    vi->visual,                         // Visual
                                    value_mask,                         // Value Mask
                                    &window_attribs);                   // Window Attributes


    XFree(vi);
    XFreeColormap(s_display, window_attribs.colormap);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 
        (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 6,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    if(!extension_supported("GLX_ARB_create_context") || glXCreateContextAttribsARB == NULL)
        s_gl_context = glXCreateNewContext(s_display, fbconfig, GLX_RGBA_TYPE, 0, True);
    else
        s_gl_context = glXCreateContextAttribsARB(s_display, fbconfig, 0, True, context_attribs);
    XSync(s_display, False);

    glXMakeCurrent(s_display, s_window.handle, s_gl_context);
    XClearWindow(s_display, s_window.handle);
    XMapRaised(s_display, s_window.handle);
    Xutf8SetWMProperties(s_display, s_window.handle, "Gem", "Gem", NULL, 0, NULL, NULL, NULL);

    s_del_win_atom = XInternAtom(s_display, "WM_DELETE_WINDOW", True);
    s_state_atom = XInternAtom(s_display, "_NET_WM_STATE", True);
    s_fullscreen_atom = XInternAtom(s_display, "_NET_WM_STATE_FULLSCREEN", True);
    s_maximizeh_atom = XInternAtom(s_display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);
    s_maximizev_atom = XInternAtom(s_display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
    XSetWMProtocols(s_display, s_window.handle, &s_del_win_atom, 1);

    XSizeHints sizeHints;
    sizeHints.flags = PMinSize;
    sizeHints.min_width = 400;
    sizeHints.min_height = 400;
    XSetWMNormalHints(s_display, s_window.handle, &sizeHints);

    int version = gladLoadGLLoader((GLADloadproc)glXGetProcAddressARB);
    GEM_ENSURE_MSG(version != 0, "Failed to load OpenGL function pointers.");

    s_window.focused = true;
    s_window.width = width;
    s_window.height = height;

    set_keymap();
    s_Initialized = true;
}

void window_destroy(void)
{
    if(s_Initialized)
    {
        s_Initialized = false;
        glXDestroyContext(s_display, s_gl_context);
        XDestroyWindow(s_display, s_window.handle);
        XCloseDisplay(s_display);
    }
}

void window_dispatch_events(void)
{
    int prev_width = s_window.width;
    int prev_height = s_window.height;
    XEvent ev;

    while(!s_window.focused || !gem_needs_redraw() || XPending(s_display))
    {
        XNextEvent(s_display, &ev);
        unsigned int scancode = ev.xkey.keycode;

        int filtered = XFilterEvent(&ev, None);

        switch(ev.type)
        {
        case FocusIn: {
            gem_request_redraw();
            s_window.focused = true;
            break;
        }
        case FocusOut: {
            s_window.focused = false;
            break;
        }
        case DestroyNotify: {
            gem_close(EXIT_SUCCESS);
            return;
        }
        case Expose: {
            gem_request_redraw();
            s_window.width = ev.xexpose.width;
            s_window.height = ev.xexpose.height;
            break;
        }
        case ClientMessage: {
            if(filtered)
                return;
            if((Atom)ev.xclient.data.l[0] == s_del_win_atom)
            {
                gem_close(EXIT_SUCCESS);
                return;
            }
            break;
        }
        case KeyPress: {
            uint16_t keycode = s_keymap[scancode];
            if(keycode != GEM_KEY_NONE)
                gem_key_press(keycode, ev.xkey.state);
            break;
        }
        case ButtonPress: {
            if(s_last_mouse.mouse_code == ev.xbutton.button && s_last_mouse.count_in_seq < 3 && 
               s_last_mouse.last_time >= ev.xbutton.time - REPEAT_INTERVAL)
                s_last_mouse.count_in_seq++;
            else
                s_last_mouse.count_in_seq = 1;
            s_last_mouse.mouse_code = ev.xbutton.button;
            s_last_mouse.last_time = ev.xbutton.time;
            gem_mouse_press(ev.xbutton.button, ev.xbutton.state, s_last_mouse.count_in_seq, ev.xbutton.x, ev.xbutton.y);
            break;
        }
        case ButtonRelease: {
            break;
        }
        }
    }

    if(prev_width != s_window.width || prev_height != s_window.height)
    {
        set_projection(s_window.width, s_window.height);
        bufwin_update_screen(s_window.width, s_window.height);
    }
}

void window_swap(void)
{
    glXSwapBuffers(s_display, s_window.handle);
}

void window_get_dims(int* width, int* height)
{
    GEM_ASSERT(width != NULL);
    GEM_ASSERT(height != NULL);
    *width = s_window.width;
    *height = s_window.height;
}

void window_toggle_fullscreen(void)
{
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = s_window.handle;
    ev.xclient.message_type = s_state_atom;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = s_fullscreen_atom;
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = 1;
    ev.xclient.data.l[4] = 0;
    XSendEvent(s_display, RootWindow(s_display, s_screen), False, ClientMessage, &ev);
}

void window_toggle_maximize(void)
{
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = s_window.handle;
    ev.xclient.message_type = s_state_atom;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = s_maximizeh_atom;
    ev.xclient.data.l[2] = s_maximizev_atom;
    ev.xclient.data.l[3] = 1;
    ev.xclient.data.l[4] = 0;
    XSendEvent(s_display, RootWindow(s_display, s_screen), False, ClientMessage, &ev);

}

static bool extension_supported(const char* extension)
{
    const char* extensions = glXQueryExtensionsString(s_display, s_screen);
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
    GLXFBConfig* configs = glXChooseFBConfig(s_display, s_screen, glx_attribs, &count);
    GEM_ENSURE_MSG(configs != 0 && count > 0, "Failed to retrieve matching framebuffer configuration.");

    bool arb_multisample = extension_supported("GLX_ARB_multisample");

    GLXFBConfig best = NULL;
    int most_samples = -1;

    for(int i = 0; i < count; ++i)
    {
        GLXFBConfig cfg = configs[i];
        XVisualInfo* vi = glXGetVisualFromFBConfig(s_display, cfg);
        if(vi == NULL)
            continue;

        int samples = 0;
        if(arb_multisample)
            glXGetFBConfigAttrib(s_display, cfg, GLX_SAMPLES, &samples);
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

static void set_keymap(void)
{
    int majorOpcode, eventBase, errorBase;
    int major = 1;
    int minor = 0;
    int supported = XkbQueryExtension(s_display, &majorOpcode, &eventBase, &errorBase, &major, &minor);
    
    int scancodeMin, scancodeMax;

    memset(s_keymap, -1, sizeof(s_keymap));

    if(supported)
    {
        XkbDescPtr desc = XkbGetMap(s_display, 0, XkbUseCoreKbd);
        XkbGetNames(s_display, XkbKeyNamesMask | XkbKeyAliasesMask, desc);

        scancodeMin = desc->min_key_code;
        scancodeMax = desc->max_key_code;
        const struct
        {
            char* name;
            int key;
        } keymap[] =
        {
            { "TLDE", GEM_KEY_GRAVE_ACCENT },
            { "AE01", GEM_KEY_1 },
            { "AE02", GEM_KEY_2 },
            { "AE03", GEM_KEY_3 },
            { "AE04", GEM_KEY_4 },
            { "AE05", GEM_KEY_5 },
            { "AE06", GEM_KEY_6 },
            { "AE07", GEM_KEY_7 },
            { "AE08", GEM_KEY_8 },
            { "AE09", GEM_KEY_9 },
            { "AE10", GEM_KEY_0 },
            { "AE11", GEM_KEY_MINUS },
            { "AE12", GEM_KEY_EQUAL },
            { "AD01", GEM_KEY_Q },
            { "AD02", GEM_KEY_W },
            { "AD03", GEM_KEY_E },
            { "AD04", GEM_KEY_R },
            { "AD05", GEM_KEY_T },
            { "AD06", GEM_KEY_Y },
            { "AD07", GEM_KEY_U },
            { "AD08", GEM_KEY_I },
            { "AD09", GEM_KEY_O },
            { "AD10", GEM_KEY_P },
            { "AD11", GEM_KEY_LEFT_BRACKET },
            { "AD12", GEM_KEY_RIGHT_BRACKET },
            { "AC01", GEM_KEY_A },
            { "AC02", GEM_KEY_S },
            { "AC03", GEM_KEY_D },
            { "AC04", GEM_KEY_F },
            { "AC05", GEM_KEY_G },
            { "AC06", GEM_KEY_H },
            { "AC07", GEM_KEY_J },
            { "AC08", GEM_KEY_K },
            { "AC09", GEM_KEY_L },
            { "AC10", GEM_KEY_SEMICOLON },
            { "AC11", GEM_KEY_APOSTROPHE },
            { "AB01", GEM_KEY_Z },
            { "AB02", GEM_KEY_X },
            { "AB03", GEM_KEY_C },
            { "AB04", GEM_KEY_V },
            { "AB05", GEM_KEY_B },
            { "AB06", GEM_KEY_N },
            { "AB07", GEM_KEY_M },
            { "AB08", GEM_KEY_COMMA },
            { "AB09", GEM_KEY_PERIOD },
            { "AB10", GEM_KEY_SLASH },
            { "BKSL", GEM_KEY_BACKSLASH },
            { "LSGT", GEM_KEY_WORLD_1 },
            { "SPCE", GEM_KEY_SPACE },
            { "ESC" , GEM_KEY_ESCAPE },
            { "RTRN", GEM_KEY_ENTER },
            { "TAB" , GEM_KEY_TAB },
            { "BKSP", GEM_KEY_BACKSPACE },
            { "INS" , GEM_KEY_INSERT },
            { "DELE", GEM_KEY_DELETE },
            { "RGHT", GEM_KEY_RIGHT },
            { "LEFT", GEM_KEY_LEFT },
            { "DOWN", GEM_KEY_DOWN },
            { "UP"  , GEM_KEY_UP },
            { "PGUP", GEM_KEY_PAGE_UP },
            { "PGDN", GEM_KEY_PAGE_DOWN },
            { "HOME", GEM_KEY_HOME },
            { "END" , GEM_KEY_END },
            { "CAPS", GEM_KEY_CAPS_LOCK },
            { "SCLK", GEM_KEY_SCROLL_LOCK },
            { "NMLK", GEM_KEY_NUM_LOCK },
            { "PRSC", GEM_KEY_PRINT_SCREEN },
            { "PAUS", GEM_KEY_PAUSE },
            { "FK01", GEM_KEY_F1 },
            { "FK02", GEM_KEY_F2 },
            { "FK03", GEM_KEY_F3 },
            { "FK04", GEM_KEY_F4 },
            { "FK05", GEM_KEY_F5 },
            { "FK06", GEM_KEY_F6 },
            { "FK07", GEM_KEY_F7 },
            { "FK08", GEM_KEY_F8 },
            { "FK09", GEM_KEY_F9 },
            { "FK10", GEM_KEY_F10 },
            { "FK11", GEM_KEY_F11 },
            { "FK12", GEM_KEY_F12 },
            { "FK13", GEM_KEY_F13 },
            { "FK14", GEM_KEY_F14 },
            { "FK15", GEM_KEY_F15 },
            { "FK16", GEM_KEY_F16 },
            { "FK17", GEM_KEY_F17 },
            { "FK18", GEM_KEY_F18 },
            { "FK19", GEM_KEY_F19 },
            { "FK20", GEM_KEY_F20 },
            { "FK21", GEM_KEY_F21 },
            { "FK22", GEM_KEY_F22 },
            { "FK23", GEM_KEY_F23 },
            { "FK24", GEM_KEY_F24 },
            { "FK25", GEM_KEY_F25 },
            { "KP0" , GEM_KEY_KP_0 },
            { "KP1" , GEM_KEY_KP_1 },
            { "KP2" , GEM_KEY_KP_2 },
            { "KP3" , GEM_KEY_KP_3 },
            { "KP4" , GEM_KEY_KP_4 },
            { "KP5" , GEM_KEY_KP_5 },
            { "KP6" , GEM_KEY_KP_6 },
            { "KP7" , GEM_KEY_KP_7 },
            { "KP8" , GEM_KEY_KP_8 },
            { "KP9" , GEM_KEY_KP_9 },
            { "KPDL", GEM_KEY_KP_DECIMAL },
            { "KPDV", GEM_KEY_KP_DIVIDE },
            { "KPMU", GEM_KEY_KP_MULTIPLY },
            { "KPSU", GEM_KEY_KP_SUBTRACT },
            { "KPAD", GEM_KEY_KP_ADD },
            { "KPEN", GEM_KEY_KP_ENTER },
            { "KPEQ", GEM_KEY_KP_EQUAL },
            { "LFSH", GEM_KEY_LEFT_SHIFT },
            { "LCTL", GEM_KEY_LEFT_CONTROL },
            { "LALT", GEM_KEY_LEFT_ALT },
            { "LWIN", GEM_KEY_LEFT_SUPER },
            { "RTSH", GEM_KEY_RIGHT_SHIFT },
            { "RCTL", GEM_KEY_RIGHT_CONTROL },
            { "RALT", GEM_KEY_RIGHT_ALT },
            { "LVL3", GEM_KEY_RIGHT_ALT },
            { "MDSW", GEM_KEY_RIGHT_ALT },
            { "RWIN", GEM_KEY_RIGHT_SUPER },
            { "MENU", GEM_KEY_MENU }
        };

        for (int scancode = scancodeMin; scancode <= scancodeMax; scancode++)
        {
            int key = GEM_KEY_NONE;

            for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++)
            {
                if (strncmp(desc->names->keys[scancode].name,
                            keymap[i].name,
                            XkbKeyNameLength) == 0)
                {
                    key = keymap[i].key;
                    break;
                }
            }

            // Fall back to key aliases in case the key name did not match
            for (int i = 0; i < desc->names->num_key_aliases; i++)
            {
                if (key != GEM_KEY_NONE)
                    break;

                if (strncmp(desc->names->key_aliases[i].real,
                            desc->names->keys[scancode].name,
                            XkbKeyNameLength) != 0)
                    continue;

                for (size_t j = 0; j < sizeof(keymap) / sizeof(keymap[0]); j++)
                {
                    if (strncmp(desc->names->key_aliases[i].alias,
                                keymap[j].name,
                                XkbKeyNameLength) == 0)
                    {
                        key = keymap[j].key;
                        break;
                    }
                }
            }

            s_keymap[scancode] = key;
        }

        XkbFreeNames(desc, XkbKeyNamesMask, True);
        XkbFreeKeyboard(desc, 0, True);
    }
    else
        XDisplayKeycodes(s_display, &scancodeMin, &scancodeMax);

    int width;
    KeySym* keysyms = XGetKeyboardMapping(s_display,
                                          scancodeMin,
                                          scancodeMax - scancodeMin + 1,
                                          &width);

    for (int scancode = scancodeMin; scancode <= scancodeMax; scancode++)
    {
        if (s_keymap[scancode] < 0)
        {
            const size_t base = (scancode - scancodeMin) * width;
            s_keymap[scancode] = translate_keysym(&keysyms[base], width);
        }
    }

    XFree(keysyms);
}

static int translate_keysym(const KeySym* keysyms, int width)
{
    if (width > 1)
    {
        switch (keysyms[1])
        {
        case XK_KP_0:           return GEM_KEY_KP_0;
        case XK_KP_1:           return GEM_KEY_KP_1;
        case XK_KP_2:           return GEM_KEY_KP_2;
        case XK_KP_3:           return GEM_KEY_KP_3;
        case XK_KP_4:           return GEM_KEY_KP_4;
        case XK_KP_5:           return GEM_KEY_KP_5;
        case XK_KP_6:           return GEM_KEY_KP_6;
        case XK_KP_7:           return GEM_KEY_KP_7;
        case XK_KP_8:           return GEM_KEY_KP_8;
        case XK_KP_9:           return GEM_KEY_KP_9;
        case XK_KP_Separator:
        case XK_KP_Decimal:     return GEM_KEY_KP_DECIMAL;
        case XK_KP_Equal:       return GEM_KEY_KP_EQUAL;
        case XK_KP_Enter:       return GEM_KEY_KP_ENTER;
        default:                break;
        }
    }

    switch (keysyms[0])
    {
    case XK_Escape:         return GEM_KEY_ESCAPE;
    case XK_Tab:            return GEM_KEY_TAB;
    case XK_Shift_L:        return GEM_KEY_LEFT_SHIFT;
    case XK_Shift_R:        return GEM_KEY_RIGHT_SHIFT;
    case XK_Control_L:      return GEM_KEY_LEFT_CONTROL;
    case XK_Control_R:      return GEM_KEY_RIGHT_CONTROL;
    case XK_Meta_L:
    case XK_Alt_L:          return GEM_KEY_LEFT_ALT;
    case XK_Mode_switch: // Mapped to Alt_R on many keyboards
    case XK_ISO_Level3_Shift: // AltGr on at least some machines
    case XK_Meta_R:
    case XK_Alt_R:          return GEM_KEY_RIGHT_ALT;
    case XK_Super_L:        return GEM_KEY_LEFT_SUPER;
    case XK_Super_R:        return GEM_KEY_RIGHT_SUPER;
    case XK_Menu:           return GEM_KEY_MENU;
    case XK_Num_Lock:       return GEM_KEY_NUM_LOCK;
    case XK_Caps_Lock:      return GEM_KEY_CAPS_LOCK;
    case XK_Print:          return GEM_KEY_PRINT_SCREEN;
    case XK_Scroll_Lock:    return GEM_KEY_SCROLL_LOCK;
    case XK_Pause:          return GEM_KEY_PAUSE;
    case XK_Delete:         return GEM_KEY_DELETE;
    case XK_BackSpace:      return GEM_KEY_BACKSPACE;
    case XK_Return:         return GEM_KEY_ENTER;
    case XK_Home:           return GEM_KEY_HOME;
    case XK_End:            return GEM_KEY_END;
    case XK_Page_Up:        return GEM_KEY_PAGE_UP;
    case XK_Page_Down:      return GEM_KEY_PAGE_DOWN;
    case XK_Insert:         return GEM_KEY_INSERT;
    case XK_Left:           return GEM_KEY_LEFT;
    case XK_Right:          return GEM_KEY_RIGHT;
    case XK_Down:           return GEM_KEY_DOWN;
    case XK_Up:             return GEM_KEY_UP;
    case XK_F1:             return GEM_KEY_F1;
    case XK_F2:             return GEM_KEY_F2;
    case XK_F3:             return GEM_KEY_F3;
    case XK_F4:             return GEM_KEY_F4;
    case XK_F5:             return GEM_KEY_F5;
    case XK_F6:             return GEM_KEY_F6;
    case XK_F7:             return GEM_KEY_F7;
    case XK_F8:             return GEM_KEY_F8;
    case XK_F9:             return GEM_KEY_F9;
    case XK_F10:            return GEM_KEY_F10;
    case XK_F11:            return GEM_KEY_F11;
    case XK_F12:            return GEM_KEY_F12;
    case XK_F13:            return GEM_KEY_F13;
    case XK_F14:            return GEM_KEY_F14;
    case XK_F15:            return GEM_KEY_F15;
    case XK_F16:            return GEM_KEY_F16;
    case XK_F17:            return GEM_KEY_F17;
    case XK_F18:            return GEM_KEY_F18;
    case XK_F19:            return GEM_KEY_F19;
    case XK_F20:            return GEM_KEY_F20;
    case XK_F21:            return GEM_KEY_F21;
    case XK_F22:            return GEM_KEY_F22;
    case XK_F23:            return GEM_KEY_F23;
    case XK_F24:            return GEM_KEY_F24;
    case XK_F25:            return GEM_KEY_F25;

                            // Numeric keypad
    case XK_KP_Divide:      return GEM_KEY_KP_DIVIDE;
    case XK_KP_Multiply:    return GEM_KEY_KP_MULTIPLY;
    case XK_KP_Subtract:    return GEM_KEY_KP_SUBTRACT;
    case XK_KP_Add:         return GEM_KEY_KP_ADD;

                            // These should have been detected in secondary keysym test above!
    case XK_KP_Insert:      return GEM_KEY_KP_0;
    case XK_KP_End:         return GEM_KEY_KP_1;
    case XK_KP_Down:        return GEM_KEY_KP_2;
    case XK_KP_Page_Down:   return GEM_KEY_KP_3;
    case XK_KP_Left:        return GEM_KEY_KP_4;
    case XK_KP_Right:       return GEM_KEY_KP_6;
    case XK_KP_Home:        return GEM_KEY_KP_7;
    case XK_KP_Up:          return GEM_KEY_KP_8;
    case XK_KP_Page_Up:     return GEM_KEY_KP_9;
    case XK_KP_Delete:      return GEM_KEY_KP_DECIMAL;
    case XK_KP_Equal:       return GEM_KEY_KP_EQUAL;
    case XK_KP_Enter:       return GEM_KEY_KP_ENTER;

                            // Last resort: Check for printable keys (should not happen if the XKB
                            // extension is available). This will give a layout dependent mapping
                            // (which is wrong, and we may miss some keys, especially on non-US
                            // keyboards), but it's better than nothing...
    case XK_a:              return GEM_KEY_A;
    case XK_b:              return GEM_KEY_B;
    case XK_c:              return GEM_KEY_C;
    case XK_d:              return GEM_KEY_D;
    case XK_e:              return GEM_KEY_E;
    case XK_f:              return GEM_KEY_F;
    case XK_g:              return GEM_KEY_G;
    case XK_h:              return GEM_KEY_H;
    case XK_i:              return GEM_KEY_I;
    case XK_j:              return GEM_KEY_J;
    case XK_k:              return GEM_KEY_K;
    case XK_l:              return GEM_KEY_L;
    case XK_m:              return GEM_KEY_M;
    case XK_n:              return GEM_KEY_N;
    case XK_o:              return GEM_KEY_O;
    case XK_p:              return GEM_KEY_P;
    case XK_q:              return GEM_KEY_Q;
    case XK_r:              return GEM_KEY_R;
    case XK_s:              return GEM_KEY_S;
    case XK_t:              return GEM_KEY_T;
    case XK_u:              return GEM_KEY_U;
    case XK_v:              return GEM_KEY_V;
    case XK_w:              return GEM_KEY_W;
    case XK_x:              return GEM_KEY_X;
    case XK_y:              return GEM_KEY_Y;
    case XK_z:              return GEM_KEY_Z;
    case XK_1:              return GEM_KEY_1;
    case XK_2:              return GEM_KEY_2;
    case XK_3:              return GEM_KEY_3;
    case XK_4:              return GEM_KEY_4;
    case XK_5:              return GEM_KEY_5;
    case XK_6:              return GEM_KEY_6;
    case XK_7:              return GEM_KEY_7;
    case XK_8:              return GEM_KEY_8;
    case XK_9:              return GEM_KEY_9;
    case XK_0:              return GEM_KEY_0;
    case XK_space:          return GEM_KEY_SPACE;
    case XK_minus:          return GEM_KEY_MINUS;
    case XK_equal:          return GEM_KEY_EQUAL;
    case XK_bracketleft:    return GEM_KEY_LEFT_BRACKET;
    case XK_bracketright:   return GEM_KEY_RIGHT_BRACKET;
    case XK_backslash:      return GEM_KEY_BACKSLASH;
    case XK_semicolon:      return GEM_KEY_SEMICOLON;
    case XK_apostrophe:     return GEM_KEY_APOSTROPHE;
    case XK_grave:          return GEM_KEY_GRAVE_ACCENT;
    case XK_comma:          return GEM_KEY_COMMA;
    case XK_period:         return GEM_KEY_PERIOD;
    case XK_slash:          return GEM_KEY_SLASH;
    case XK_less:           return GEM_KEY_WORLD_1; // At least in some layouts...
    default:                break;
    }

    // No matching translation was found
    return GEM_KEY_NONE;
}
