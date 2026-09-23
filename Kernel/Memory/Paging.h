#pragma once
#include "Types.h"
#include "Config.h"

namespace Paging {

constexpr size_t PAGE_SIZE = 4 * KIB;

/// There is deliberately no writable-and-executable option.
enum class Access : uint8_t {
    READ_ONLY,
    READ_WRITE,
    READ_EXECUTE,
};

enum class Caching : uint8_t {
    NORMAL,
    DISABLED,  ///< Memory mapped device registers must not be cached.
};

/// Every frame is reachable at this offset, so a physical address never needs a
/// mapping of its own.
inline uintptr_t ToVirtual(uintptr_t InPhysical) {
    return InPhysical + Config::DIRECT_MAP_BASE;
}

inline uintptr_t ToPhysical(uintptr_t InVirtual) {
    return InVirtual - Config::DIRECT_MAP_BASE;
}

void Initialize();

bool Map(uintptr_t InVirtual, uintptr_t InPhysical, size_t InSize, Access InAccess,
         Caching InCaching = Caching::NORMAL);

/// Physical address backing InVirtual, or 0 when nothing is mapped there.
uintptr_t Translate(uintptr_t InVirtual);

bool IsWritable(uintptr_t InVirtual);
bool IsExecutable(uintptr_t InVirtual);

void Dump();

}  // namespace Paging
