#pragma once
#include "Types.h"

namespace Cpu {

// Feature discovery via the CPUID instruction.
namespace Id {

struct Result {
    uint32_t Eax;
    uint32_t Ebx;
    uint32_t Ecx;
    uint32_t Edx;
};

inline Result Query(uint32_t InLeaf, uint32_t InSubLeaf = 0) {
    Result result{};
    asm volatile("cpuid"
                 : "=a"(result.Eax), "=b"(result.Ebx), "=c"(result.Ecx), "=d"(result.Edx)
                 : "a"(InLeaf), "c"(InSubLeaf));
    return result;
}

enum class Leaf : uint32_t {
    BASIC_INFO = 0x0,
    FEATURES = 0x1,
    EXTENDED_MAX = 0x8000'0000,
    EXTENDED_FEATURES = 0x8000'0001,
    BRAND_STRING_0 = 0x8000'0002,
};

/// CPUID.01H:EDX bits
enum class FeatureEdx : uint32_t {
    FPU = 1U << 0,
    PSE = 1U << 3,
    TSC = 1U << 4,
    MSR = 1U << 5,
    PAE = 1U << 6,
    APIC = 1U << 9,
    PGE = 1U << 13,
    MMX = 1U << 23,
    SSE = 1U << 25,
    SSE2 = 1U << 26,
};

/// CPUID.01H:ECX bits
enum class FeatureEcx : uint32_t {
    SSE3 = 1U << 0,
    SSSE3 = 1U << 9,
    SSE4_1 = 1U << 19,
    SSE4_2 = 1U << 23,
    X2APIC = 1U << 21,
    TSC_DEADLINE = 1U << 24,
    XSAVE = 1U << 26,
    HYPERVISOR = 1U << 31,
};

/// CPUID.80000001H:EDX bits
enum class ExtendedFeatureEdx : uint32_t {
    NX = 1U << 20,
    GB_PAGES = 1U << 26,
    RDTSCP = 1U << 27,
    LONG_MODE = 1U << 29,
};

inline bool Has(FeatureEdx InFeature) {
    return (Query(ToUnderlying(Leaf::FEATURES)).Edx & ToUnderlying(InFeature)) != 0;
}

inline bool Has(FeatureEcx InFeature) {
    return (Query(ToUnderlying(Leaf::FEATURES)).Ecx & ToUnderlying(InFeature)) != 0;
}

inline bool Has(ExtendedFeatureEdx InFeature) {
    if (Query(ToUnderlying(Leaf::EXTENDED_MAX)).Eax < ToUnderlying(Leaf::EXTENDED_FEATURES)) {
        return false;
    }
    return (Query(ToUnderlying(Leaf::EXTENDED_FEATURES)).Edx & ToUnderlying(InFeature)) != 0;
}

/// Writes the 12 byte vendor string plus terminator into OutVendor.
inline void GetVendor(char OutVendor[13]) {
    const Result result = Query(ToUnderlying(Leaf::BASIC_INFO));
    const uint32_t words[3] = {result.Ebx, result.Edx, result.Ecx};
    for (unsigned word = 0; word < 3; word++) {
        for (unsigned byte = 0; byte < 4; byte++) {
            OutVendor[word * 4 + byte] = static_cast<char>((words[word] >> (byte * 8)) & 0xff);
        }
    }
    OutVendor[12] = '\0';
}

}  // namespace Id
}  // namespace Cpu
