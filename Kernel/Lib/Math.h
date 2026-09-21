#pragma once
#include "Types.h"

namespace Math {

template <typename T>
constexpr T Abs(T InValue) {
    return InValue >= 0 ? InValue : -InValue;
}

template <typename T>
constexpr T Min(T InLeft, T InRight) {
    return InLeft > InRight ? InRight : InLeft;
}

template <typename T>
constexpr T Max(T InLeft, T InRight) {
    return InLeft > InRight ? InLeft : InRight;
}

template <typename T>
constexpr T AlignUp(T InValue, T InAlignment) {
    return (InValue + InAlignment - 1) / InAlignment * InAlignment;
}

template <typename T>
constexpr T AlignDown(T InValue, T InAlignment) {
    return InValue / InAlignment * InAlignment;
}

}  // namespace Math
