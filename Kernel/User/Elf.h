#pragma once
#include "Types.h"

// Just enough of ELF64 to walk the program headers of a static executable.
namespace Elf {

constexpr uint8_t CLASS_64 = 2;
constexpr uint8_t DATA_LITTLE_ENDIAN = 1;
constexpr uint16_t TYPE_EXECUTABLE = 2;
constexpr uint16_t MACHINE_X86_64 = 0x3e;

constexpr uint32_t PT_LOAD = 1;

constexpr uint32_t PF_EXECUTE = 1 << 0;
constexpr uint32_t PF_WRITE = 1 << 1;
constexpr uint32_t PF_READ = 1 << 2;

struct Header {
    uint8_t Magic[4];  ///< 0x7f 'E' 'L' 'F'
    uint8_t Class;
    uint8_t Data;
    uint8_t IdentVersion;
    uint8_t Abi;
    uint8_t AbiVersion;
    uint8_t Padding[7];
    uint16_t Type;
    uint16_t Machine;
    uint32_t Version;
    uint64_t Entry;
    uint64_t ProgramHeaderOffset;
    uint64_t SectionHeaderOffset;
    uint32_t Flags;
    uint16_t HeaderSize;
    uint16_t ProgramHeaderEntrySize;
    uint16_t ProgramHeaderCount;
    uint16_t SectionHeaderEntrySize;
    uint16_t SectionHeaderCount;
    uint16_t SectionNameIndex;
} __attribute__((packed));
static_assert(sizeof(Header) == 64, "Elf::Header has the wrong size");

struct ProgramHeader {
    uint32_t Type;
    uint32_t Flags;
    uint64_t Offset;
    uint64_t VirtualAddress;
    uint64_t PhysicalAddress;
    uint64_t FileSize;
    uint64_t MemorySize;
    uint64_t Alignment;
} __attribute__((packed));
static_assert(sizeof(ProgramHeader) == 56, "Elf::ProgramHeader has the wrong size");

}  // namespace Elf
