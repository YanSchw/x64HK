#include "Lib/String.h"

extern "C" char* strchrnul(const char* InString, int InChar) {
    if (InString != nullptr) {
        while (*InString != '\0' && *InString != InChar) {
            InString++;
        }
    }
    return const_cast<char*>(InString);
}

extern "C" char* strchr(const char* InString, int InChar) {
    if (InString != nullptr) {
        const char* found = strchrnul(InString, InChar);
        if (*found == InChar) {
            return const_cast<char*>(found);
        }
    }
    return nullptr;
}

extern "C" int strcmp(const char* InLeft, const char* InRight) {
    if (InLeft == nullptr || InRight == nullptr) {
        return 0;
    }
    while (*InLeft == *InRight && *InLeft != '\0') {
        InLeft++;
        InRight++;
    }
    return static_cast<int>(*InLeft) - static_cast<int>(*InRight);
}

extern "C" int strncmp(const char* InLeft, const char* InRight, size_t InCount) {
    if (InLeft == nullptr || InRight == nullptr) {
        return 0;
    }
    for (size_t i = 0; i < InCount; i++) {
        if (InLeft[i] != InRight[i]) {
            return static_cast<int>(InLeft[i]) - static_cast<int>(InRight[i]);
        }
        if (InLeft[i] == '\0') {
            break;
        }
    }
    return 0;
}

extern "C" size_t strlen(const char* InString) {
    size_t length = 0;
    if (InString != nullptr) {
        while (InString[length] != '\0') {
            length++;
        }
    }
    return length;
}

extern "C" char* strcpy(char* OutDest, const char* InSource) {
    char* result = OutDest;
    if (OutDest != nullptr && InSource != nullptr) {
        while ((*OutDest++ = *InSource++) != '\0') {
        }
    }
    return result;
}

extern "C" char* strncpy(char* OutDest, const char* InSource, size_t InCount) {
    char* result = OutDest;
    if (OutDest != nullptr && InSource != nullptr) {
        while (InCount-- != 0 && (*OutDest++ = *InSource++) != '\0') {
        }
    }
    return result;
}

extern "C" void* memcpy(void* __restrict__ OutDest, const void* __restrict__ InSource, size_t InSize) {
    uint8_t* destination = static_cast<uint8_t*>(OutDest);
    const uint8_t* source = static_cast<const uint8_t*>(InSource);
    for (size_t i = 0; i < InSize; i++) {
        destination[i] = source[i];
    }
    return OutDest;
}

extern "C" void* memmove(void* OutDest, const void* InSource, size_t InSize) {
    uint8_t* destination = static_cast<uint8_t*>(OutDest);
    const uint8_t* source = static_cast<const uint8_t*>(InSource);
    if (source > destination) {
        for (size_t i = 0; i < InSize; i++) {
            destination[i] = source[i];
        }
    } else {
        for (size_t i = InSize; i != 0; i--) {
            destination[i - 1] = source[i - 1];
        }
    }
    return OutDest;
}

extern "C" void* memset(void* OutDest, int InPattern, size_t InSize) {
    uint8_t* destination = static_cast<uint8_t*>(OutDest);
    for (size_t i = 0; i < InSize; i++) {
        destination[i] = static_cast<uint8_t>(InPattern);
    }
    return OutDest;
}

extern "C" int memcmp(const void* InLeft, const void* InRight, size_t InSize) {
    const uint8_t* left = static_cast<const uint8_t*>(InLeft);
    const uint8_t* right = static_cast<const uint8_t*>(InRight);
    for (size_t i = 0; i < InSize; i++) {
        if (left[i] != right[i]) {
            return static_cast<int>(left[i]) - static_cast<int>(right[i]);
        }
    }
    return 0;
}
