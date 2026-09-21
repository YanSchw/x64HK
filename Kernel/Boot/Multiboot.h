#pragma once
#include "Types.h"

// Boot loader handover.
//
// Multiboot2 is the target: it is the only handover that gives us the ACPI RSDP
// (so ACPI needs no memory scan) and a full memory map. Multiboot1 is still
// understood because QEMU's `-kernel` loader speaks nothing else, and booting
// the raw ELF is much faster to iterate on than rebuilding an ISO.
namespace Multiboot {

enum class Standard {
    NONE,  ///< Started by something that is not Multiboot compliant
    V1,
    V2,
};

enum class MemoryType : uint32_t {
    AVAILABLE = 1,
    RESERVED = 2,
    ACPI_RECLAIMABLE = 3,  ///< Usable once the ACPI tables have been consumed
    ACPI_NON_VOLATILE = 4,
    BAD = 5,
};

struct MemoryRegion {
    uint64_t Address;
    uint64_t Length;
    MemoryType Type;

    uint64_t End() const { return Address + Length; }
};

struct Module {
    uintptr_t Start;
    uintptr_t End;
    const char* CommandLine;

    size_t Size() const { return End - Start; }
};

struct Framebuffer {
    uint64_t Address;
    uint32_t Pitch;  ///< Bytes per scanline, not pixels
    uint32_t Width;
    uint32_t Height;
    uint8_t BitsPerPixel;
    uint8_t Type;  ///< 0 indexed, 1 RGB, 2 EGA text
};

/// Parses whatever the boot loader left in MultibootMagic / MultibootInfo.
bool Initialize();

Standard GetStandard();
const char* GetCommandLine();
const char* GetBootLoaderName();

/// Physical address of the RSDP as reported by the loader, or 0.
uintptr_t GetAcpiRsdp();

unsigned GetMemoryRegionCount();
bool GetMemoryRegion(unsigned InIndex, MemoryRegion& OutRegion);

/// Total bytes in regions marked available.
uint64_t GetAvailableMemory();

unsigned GetModuleCount();
bool GetModule(unsigned InIndex, Module& OutModule);

const Framebuffer* GetFramebuffer();

void Dump();

}  // namespace Multiboot
