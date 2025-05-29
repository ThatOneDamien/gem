#pragma once
#include "app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#ifdef GEM_DEBUG
    #include <signal.h>
    #define GEM_ERROR(msg)                \
        {                                 \
            fprintf(stderr, "\033[31m");  \
            fprintf(stderr, msg);         \
            fprintf(stderr, "\033[0m\n"); \
            raise(SIGTRAP);               \
        }
    #define GEM_ERROR_ARGS(msg, ...)           \
        {                                      \
            fprintf(stderr, "\033[31m");       \
            fprintf(stderr, msg, __VA_ARGS__); \
            fprintf(stderr, "\033[0m\n");      \
            raise(SIGTRAP);                    \
        }
    #define GEM_ASSERT(cond)                                                               \
        {                                                                                  \
            if(!(cond))                                                                    \
                GEM_ERROR_ARGS("Assertion failed (%s: %d): %s", __FILE__, __LINE__, #cond) \
        }
    #define GEM_ASSERT_MSG(cond, msg) \
        {                             \
            if(!(cond))               \
                GEM_ERROR(msg)        \
        }
    #define GEM_ASSERT_ARGS(cond, msg, ...)      \
        {                                        \
            if(!(cond))                          \
                GEM_ERROR_ARGS(msg, __VA_ARGS__) \
        }
#else
    #define GEM_ERROR(msg)
    #define GEM_ERROR_ARGS(msg, ...)
    #define GEM_ASSERT(cond)
    #define GEM_ASSERT_MSG(cond, msg)
    #define GEM_ASSERT_ARGS(cond, msg, ...)
#endif

#define GEM_ENSURE(cond)                                                            \
    {                                                                               \
        if(!(cond))                                                                 \
        {                                                                           \
            GEM_ERROR_ARGS("Ensure failed (%s: %d): %s", __FILE__, __LINE__, #cond) \
            gem_close(EXIT_FAILURE);                                                \
        }                                                                           \
    }
#define GEM_ENSURE_MSG(cond, msg)       \
    {                                   \
        if(!(cond))                     \
        {                               \
            GEM_ERROR(msg)              \
            gem_close(EXIT_FAILURE);    \
        }                               \
    }
#define GEM_ENSURE_ARGS(cond, msg, ...)         \
    {                                           \
        if(!(cond))                             \
        {                                       \
            GEM_ERROR_ARGS(msg, __VA_ARGS__)    \
            gem_close(EXIT_FAILURE);            \
        }                                       \
    }

#ifdef __GNUC__
    #define alignas(alignment) __attribute__((packed,aligned(alignment)))
    #define UNUSED __attribute__((unused))
#else
    #define alignas(alignment) 
    #define UNUSED
#endif
