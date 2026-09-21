#pragma once

// Fixed width integer types. Freestanding builds get no <cstdint>, so the
// widths are pinned down with static_asserts below.
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using uintptr_t = unsigned long long;

using int8_t = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;
using intptr_t = long long;

using size_t = __SIZE_TYPE__;
using ssize_t = long;
using ptrdiff_t = __PTRDIFF_TYPE__;

static_assert(sizeof(int8_t) == 1, "Wrong size for 'int8_t'");
static_assert(sizeof(int16_t) == 2, "Wrong size for 'int16_t'");
static_assert(sizeof(int32_t) == 4, "Wrong size for 'int32_t'");
static_assert(sizeof(int64_t) == 8, "Wrong size for 'int64_t'");
static_assert(sizeof(uint8_t) == 1, "Wrong size for 'uint8_t'");
static_assert(sizeof(uint16_t) == 2, "Wrong size for 'uint16_t'");
static_assert(sizeof(uint32_t) == 4, "Wrong size for 'uint32_t'");
static_assert(sizeof(uint64_t) == 8, "Wrong size for 'uint64_t'");
static_assert(sizeof(uintptr_t) == sizeof(void*), "Wrong size for 'uintptr_t'");
static_assert(sizeof(intptr_t) == sizeof(void*), "Wrong size for 'intptr_t'");

constexpr int8_t INT8_MIN = -__INT8_MAX__ - 1;
constexpr int8_t INT8_MAX = __INT8_MAX__;
constexpr int16_t INT16_MIN = -__INT16_MAX__ - 1;
constexpr int16_t INT16_MAX = __INT16_MAX__;
constexpr int32_t INT32_MIN = -__INT32_MAX__ - 1;
constexpr int32_t INT32_MAX = __INT32_MAX__;
constexpr int64_t INT64_MIN = -__INT64_MAX__ - 1;
constexpr int64_t INT64_MAX = __INT64_MAX__;

constexpr uint8_t UINT8_MAX = __UINT8_MAX__;
constexpr uint16_t UINT16_MAX = __UINT16_MAX__;
constexpr uint32_t UINT32_MAX = __UINT32_MAX__;
constexpr uint64_t UINT64_MAX = __UINT64_MAX__;
constexpr uintptr_t UINTPTR_MAX = __UINTPTR_MAX__;
constexpr size_t SIZE_MAX = __SIZE_MAX__;
constexpr ptrdiff_t PTRDIFF_MAX = __PTRDIFF_MAX__;
constexpr ptrdiff_t PTRDIFF_MIN = -__PTRDIFF_MAX__ - 1;

constexpr size_t KIB = 1024;
constexpr size_t MIB = 1024 * KIB;
constexpr size_t GIB = 1024 * MIB;

// Scoped enums are used heavily for hardware register fields; this is the
// std::to_underlying equivalent for a freestanding build.
template <typename EnumType>
struct UnderlyingType {
    using Type = __underlying_type(EnumType);
};

template <typename EnumType>
constexpr typename UnderlyingType<EnumType>::Type ToUnderlying(EnumType InValue) {
    return static_cast<typename UnderlyingType<EnumType>::Type>(InValue);
}

template <typename T, size_t N>
constexpr size_t ArraySize(T (&)[N]) {
    return N;
}
