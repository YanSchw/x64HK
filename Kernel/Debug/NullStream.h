#pragma once
#include "Types.h"
#include "Lib/OutputStream.h"

// Swallows everything streamed into it. Used for DBG_VERBOSE in non-verbose
// builds, where the whole expression then optimises away.
class NullStream {
public:
    constexpr NullStream() = default;

    template <typename T>
    NullStream& operator<<(T InValue) {
        // Keeps the argument type-checked, so a verbose build cannot suddenly
        // fail to compile.
        TypeCheck(InValue);
        return *this;
    }

private:
    template <typename T>
    auto TypeCheck(T InValue, OutputStream* InProbe = nullptr) -> decltype(*InProbe << InValue, void()) {}
};

extern NullStream g_NullStream;
