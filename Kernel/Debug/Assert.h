#pragma once
#include "Types.h"

#ifndef STRINGIFY
#define STRINGIFY(S) #S
#endif

/// Compile time size check, mostly for hardware layouts.
#define ASSERT_SIZE(TYPE, SIZE) \
    static_assert(sizeof(TYPE) == (SIZE), "Wrong size for " STRINGIFY(TYPE))

#ifdef NDEBUG
#define ASSERT(EXPRESSION) ((void)0)
#else
// The expression is evaluated exactly once and only in debug builds, so it must
// stay free of side effects.
#define ASSERT(EXPRESSION)                                                      \
    do {                                                                        \
        if (__builtin_expect(!(EXPRESSION), 0)) {                               \
            AssertionFailed(STRINGIFY(EXPRESSION), __func__, __FILE__, __LINE__); \
        }                                                                       \
    } while (false)
#endif

[[noreturn]] void AssertionFailed(const char* InExpression, const char* InFunction, const char* InFile,
                                  int InLine);
