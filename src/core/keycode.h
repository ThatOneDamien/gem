#pragma once

// Key macros
#define GEM_KEY_NONE               0

// Printable characters
#define GEM_KEY_SPACE              32
#define GEM_KEY_APOSTROPHE         39  /* ' */
#define GEM_KEY_COMMA              44  /* , */
#define GEM_KEY_MINUS              45  /* - */
#define GEM_KEY_PERIOD             46  /* . */
#define GEM_KEY_SLASH              47  /* / */
#define GEM_KEY_0                  48
#define GEM_KEY_1                  49
#define GEM_KEY_2                  50
#define GEM_KEY_3                  51
#define GEM_KEY_4                  52
#define GEM_KEY_5                  53
#define GEM_KEY_6                  54
#define GEM_KEY_7                  55
#define GEM_KEY_8                  56
#define GEM_KEY_9                  57
#define GEM_KEY_SEMICOLON          59  /* ; */
#define GEM_KEY_EQUAL              61  /* = */
#define GEM_KEY_LEFT_BRACKET       91  /* [ */
#define GEM_KEY_BACKSLASH          92  /* \ */
#define GEM_KEY_RIGHT_BRACKET      93  /* ] */
#define GEM_KEY_GRAVE_ACCENT       96  /* ` */
#define GEM_KEY_TILDE              96  /* ~ (Alias for `) */
#define GEM_KEY_A                  97
#define GEM_KEY_B                  98
#define GEM_KEY_C                  99
#define GEM_KEY_D                  100
#define GEM_KEY_E                  101
#define GEM_KEY_F                  102
#define GEM_KEY_G                  103
#define GEM_KEY_H                  104
#define GEM_KEY_I                  105
#define GEM_KEY_J                  106
#define GEM_KEY_K                  107
#define GEM_KEY_L                  108
#define GEM_KEY_M                  109
#define GEM_KEY_N                  110
#define GEM_KEY_O                  111
#define GEM_KEY_P                  112
#define GEM_KEY_Q                  113
#define GEM_KEY_R                  114
#define GEM_KEY_S                  115
#define GEM_KEY_T                  116
#define GEM_KEY_U                  117
#define GEM_KEY_V                  118
#define GEM_KEY_W                  119
#define GEM_KEY_X                  120
#define GEM_KEY_Y                  121
#define GEM_KEY_Z                  122
#define GEM_KEY_WORLD_1            161 /* non-US #1 */
#define GEM_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define GEM_KEY_ESCAPE             256
#define GEM_KEY_ENTER              257
#define GEM_KEY_TAB                258
#define GEM_KEY_BACKSPACE          259
#define GEM_KEY_INSERT             260
#define GEM_KEY_DELETE             261
#define GEM_KEY_RIGHT              262
#define GEM_KEY_LEFT               263
#define GEM_KEY_DOWN               264
#define GEM_KEY_UP                 265
#define GEM_KEY_PAGE_UP            266
#define GEM_KEY_PAGE_DOWN          267
#define GEM_KEY_HOME               268
#define GEM_KEY_END                269
#define GEM_KEY_CAPS_LOCK          280
#define GEM_KEY_SCROLL_LOCK        281
#define GEM_KEY_NUM_LOCK           282
#define GEM_KEY_PRINT_SCREEN       283
#define GEM_KEY_PAUSE              284
#define GEM_KEY_F1                 290
#define GEM_KEY_F2                 291
#define GEM_KEY_F3                 292
#define GEM_KEY_F4                 293
#define GEM_KEY_F5                 294
#define GEM_KEY_F6                 295
#define GEM_KEY_F7                 296
#define GEM_KEY_F8                 297
#define GEM_KEY_F9                 298
#define GEM_KEY_F10                299
#define GEM_KEY_F11                300
#define GEM_KEY_F12                301
#define GEM_KEY_F13                302
#define GEM_KEY_F14                303
#define GEM_KEY_F15                304
#define GEM_KEY_F16                305
#define GEM_KEY_F17                306
#define GEM_KEY_F18                307
#define GEM_KEY_F19                308
#define GEM_KEY_F20                309
#define GEM_KEY_F21                310
#define GEM_KEY_F22                311
#define GEM_KEY_F23                312
#define GEM_KEY_F24                313
#define GEM_KEY_F25                314
#define GEM_KEY_KP_0               320
#define GEM_KEY_KP_1               321
#define GEM_KEY_KP_2               322
#define GEM_KEY_KP_3               323
#define GEM_KEY_KP_4               324
#define GEM_KEY_KP_5               325
#define GEM_KEY_KP_6               326
#define GEM_KEY_KP_7               327
#define GEM_KEY_KP_8               328
#define GEM_KEY_KP_9               329
#define GEM_KEY_KP_DECIMAL         330
#define GEM_KEY_KP_DIVIDE          331
#define GEM_KEY_KP_MULTIPLY        332
#define GEM_KEY_KP_SUBTRACT        333
#define GEM_KEY_KP_ADD             334
#define GEM_KEY_KP_ENTER           335
#define GEM_KEY_KP_EQUAL           336
#define GEM_KEY_LEFT_SHIFT         340
#define GEM_KEY_LEFT_CONTROL       341
#define GEM_KEY_LEFT_ALT           342
#define GEM_KEY_LEFT_SUPER         343
#define GEM_KEY_RIGHT_SHIFT        344
#define GEM_KEY_RIGHT_CONTROL      345
#define GEM_KEY_RIGHT_ALT          346
#define GEM_KEY_RIGHT_SUPER        347
#define GEM_KEY_MENU               348

#define GEM_KEY_LAST               GEM_KEY_MENU 

// Key mod macros
#define GEM_MOD_SHIFT              (1 << 0)
#define GEM_MOD_CAPS               (1 << 1)
#define GEM_MOD_CONTROL            (1 << 2)

// Mouse button macros
#define GEM_MOUSE_BUTTON_NONE      0
#define GEM_MOUSE_BUTTON_1         1
#define GEM_MOUSE_BUTTON_2         2
#define GEM_MOUSE_BUTTON_3         3
#define GEM_MOUSE_BUTTON_4         4
#define GEM_MOUSE_BUTTON_5         5
#define GEM_MOUSE_BUTTON_6         6
#define GEM_MOUSE_BUTTON_7         7
#define GEM_MOUSE_BUTTON_8         8
#define GEM_MOUSE_BUTTON_LAST      GEM_MOUSE_BUTTON_8
#define GEM_MOUSE_BUTTON_LEFT      GEM_MOUSE_BUTTON_1
#define GEM_MOUSE_BUTTON_RIGHT     GEM_MOUSE_BUTTON_2
#define GEM_MOUSE_BUTTON_MIDDLE    GEM_MOUSE_BUTTON_3

