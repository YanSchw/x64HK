#pragma once
#include "Types.h"

namespace Cpu {

enum class MsrIndex : uint32_t {
    PLATFORM_INFO = 0xce,
    APIC_BASE = 0x1b,
    TSC_DEADLINE = 0x6e0,
    EFER = 0xc000'0080,      ///< Extended Feature Enable Register
    STAR = 0xc000'0081,      ///< syscall/sysret segment bases
    LSTAR = 0xc000'0082,     ///< syscall entry rip
    SFMASK = 0xc000'0084,    ///< rflags cleared on syscall
    FS_BASE = 0xc000'0100,
    GS_BASE = 0xc000'0101,
    KERNEL_GS_BASE = 0xc000'0102,  ///< Swapped in by swapgs
};

enum class MsrEfer : uint64_t {
    SCE = 1ULL << 0,    ///< syscall/sysret enabled
    LME = 1ULL << 8,    ///< Long mode enable
    LMA = 1ULL << 10,   ///< Long mode active (read only)
    NXE = 1ULL << 11,   ///< No-execute page bit enabled
};

/// rdmsr/wrmsr split the 64-bit value across edx:eax.
template <MsrIndex INDEX>
class Msr {
public:
    static uint64_t Read() {
        uint32_t low, high;
        asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(ToUnderlying(INDEX)));
        return (static_cast<uint64_t>(high) << 32) | low;
    }

    static void Write(uint64_t InValue) {
        asm volatile("wrmsr"
                     :
                     : "c"(ToUnderlying(INDEX)), "a"(static_cast<uint32_t>(InValue)),
                       "d"(static_cast<uint32_t>(InValue >> 32)));
    }
};

}  // namespace Cpu
