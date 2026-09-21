#pragma once
#include "Types.h"

namespace Cpu {

enum class CR0 : uint64_t {
    PE = 1ULL << 0,   ///< Protected mode enabled
    MP = 1ULL << 1,   ///< Monitor co-processor
    EM = 1ULL << 2,   ///< Emulation, i.e. no x87 FPU present
    TS = 1ULL << 3,   ///< Task switched
    ET = 1ULL << 4,   ///< Extension type
    NE = 1ULL << 5,   ///< Numeric error
    WP = 1ULL << 16,  ///< Write protect (honour read-only pages in ring 0)
    AM = 1ULL << 18,  ///< Alignment mask
    NW = 1ULL << 29,  ///< Not write-through
    CD = 1ULL << 30,  ///< Cache disable
    PG = 1ULL << 31,  ///< Paging
};

enum class CR4 : uint64_t {
    VME = 1ULL << 0,
    PVI = 1ULL << 1,
    TSD = 1ULL << 2,
    DE = 1ULL << 3,
    PSE = 1ULL << 4,         ///< Page size extension
    PAE = 1ULL << 5,         ///< Physical address extension, required for long mode
    MCE = 1ULL << 6,
    PGE = 1ULL << 7,         ///< Global pages survive a CR3 reload
    PCE = 1ULL << 8,
    OSFXSR = 1ULL << 9,
    OSXMMEXCPT = 1ULL << 10,
    UMIP = 1ULL << 11,
    FSGSBASE = 1ULL << 16,
    PCIDE = 1ULL << 17,
    OSXSAVE = 1ULL << 18,
    SMEP = 1ULL << 20,
    SMAP = 1ULL << 21,
};

/// Typed access to CR0/CR2/CR3/CR4. The register index has to be an immediate,
/// hence the template parameter and the %c modifier.
template <uint8_t INDEX>
class ControlRegister {
public:
    static uintptr_t Read() {
        uintptr_t value;
        asm volatile("mov %%cr%c1, %0" : "=r"(value) : "n"(INDEX));
        return value;
    }

    static void Write(uintptr_t InValue) { asm volatile("mov %0, %%cr%c1" : : "r"(InValue), "n"(INDEX)); }
};

using CR2 = ControlRegister<2>;
using CR3 = ControlRegister<3>;

}  // namespace Cpu
