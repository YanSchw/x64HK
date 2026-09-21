#pragma once
#include "Types.h"

// The compiler lowers struct copies and array initialisation to calls into
// these, so they have to exist with exactly these C names even though nothing
// in the kernel calls them directly.
extern "C" {
char* strchrnul(const char* InString, int InChar);
char* strchr(const char* InString, int InChar);
int strcmp(const char* InLeft, const char* InRight);
int strncmp(const char* InLeft, const char* InRight, size_t InCount);
size_t strlen(const char* InString);
char* strcpy(char* OutDest, const char* InSource);
char* strncpy(char* OutDest, const char* InSource, size_t InCount);
void* memcpy(void* __restrict__ OutDest, const void* __restrict__ InSource, size_t InSize);
void* memmove(void* OutDest, const void* InSource, size_t InSize);
void* memset(void* OutDest, int InPattern, size_t InSize);
int memcmp(const void* InLeft, const void* InRight, size_t InSize);
}
