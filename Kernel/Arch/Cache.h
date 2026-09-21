#pragma once
#include "Types.h"

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

/// Pads a type out to a full cache line so that neighbouring per-core data does
/// not share a line (false sharing turns a plain store into a bus transaction).
#define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)
