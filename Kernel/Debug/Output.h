#pragma once
#include "Types.h"
#include "Lib/OutputStream.h"
#include "Debug/NullStream.h"

// Kernel-wide debug output.
//
// Each core gets its own stream so that concurrent output does not interleave
// mid-line. Until Main registers them, everything goes to a bare COM1 writer
// that needs no construction, which makes DBG usable from the very first
// instruction of KernelInit.
namespace Debug {

OutputStream& Out();

/// Registers the stream a core should use from now on. Pass nullptr to fall
/// back to the early serial writer.
void SetStream(unsigned InCoreId, OutputStream* InStream);

}  // namespace Debug

#define DBG (Debug::Out())

#ifdef VERBOSE
#define DBG_VERBOSE (Debug::Out() << __FILE__ << ":" << __LINE__ << " ")
#else
#define DBG_VERBOSE (g_NullStream)
#endif
