#pragma once
#include "Types.h"

/// Reports an unrecoverable condition and parks the current core forever.
#define PANIC(MESSAGE)                                                                        \
    do {                                                                                      \
        DBG << "PANIC: " << (MESSAGE) << " in " << __func__ << " at " << __FILE__ << ":"      \
            << __LINE__ << EndLine;                                                           \
        Debug::RequestExit(Debug::ExitCode::FAILURE);                                         \
        Cpu::Die();                                                                           \
    } while (false)

// Included last so the macro above is already visible inside these headers.
#include "Arch/Cpu.h"
#include "Debug/DebugExit.h"
#include "Debug/Output.h"
