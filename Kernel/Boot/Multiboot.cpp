#include "Boot/Multiboot.h"
#include "Debug/Output.h"

// Written by Boot/Entry.asm from eax/ebx before anything else runs.
extern "C" uint32_t MultibootMagic = 0;
extern "C" uint32_t MultibootInfo = 0;

namespace Multiboot {

constexpr uint32_t LOADER_MAGIC_V1 = 0x2bad'b002;
constexpr uint32_t LOADER_MAGIC_V2 = 0x36d7'6289;

// --- Multiboot2 -------------------------------------------------------------

namespace V2 {

enum TagType : uint32_t {
    END = 0,
    CMDLINE = 1,
    BOOT_LOADER_NAME = 2,
    MODULE = 3,
    BASIC_MEMINFO = 4,
    MEMORY_MAP = 6,
    FRAMEBUFFER = 8,
    ACPI_OLD = 14,
    ACPI_NEW = 15,
};

struct Tag {
    uint32_t Type;
    uint32_t Size;

    /// Tags are padded to an 8 byte boundary, and Size excludes that padding.
    const Tag* Next() const {
        const uintptr_t next = (reinterpret_cast<uintptr_t>(this) + Size + 7) & ~uintptr_t{7};
        return reinterpret_cast<const Tag*>(next);
    }
} __attribute__((packed));

struct StringTag : Tag {
    char String[];
} __attribute__((packed));

struct ModuleTag : Tag {
    uint32_t Start;
    uint32_t End;
    char CommandLine[];
} __attribute__((packed));

struct MemoryMapTag : Tag {
    /// Read from the tag rather than assumed: later revisions may grow entries.
    uint32_t EntrySize;
    uint32_t EntryVersion;
    uint8_t Entries[];
} __attribute__((packed));

struct MemoryMapEntry {
    uint64_t Address;
    uint64_t Length;
    uint32_t Type;
    uint32_t Reserved;
} __attribute__((packed));

struct FramebufferTag : Tag {
    uint64_t Address;
    uint32_t Pitch;
    uint32_t Width;
    uint32_t Height;
    uint8_t BitsPerPixel;
    uint8_t FramebufferType;
    uint16_t Reserved;
} __attribute__((packed));

struct AcpiTag : Tag {
    uint8_t Rsdp[];
} __attribute__((packed));

struct InfoHeader {
    uint32_t TotalSize;
    uint32_t Reserved;
} __attribute__((packed));

}  // namespace V2

// --- Multiboot1 -------------------------------------------------------------

namespace V1 {

enum Flags : uint32_t {
    MEMORY = 1 << 0,
    BOOT_DEVICE = 1 << 1,
    CMDLINE = 1 << 2,
    MODULES = 1 << 3,
    MEMORY_MAP = 1 << 6,
    BOOT_LOADER_NAME = 1 << 9,
};

struct Info {
    uint32_t Flags;
    uint32_t MemoryLower;
    uint32_t MemoryUpper;
    uint32_t BootDevice;
    uint32_t CommandLine;
    uint32_t ModuleCount;
    uint32_t ModuleAddress;
    uint32_t Syms[4];
    uint32_t MemoryMapLength;
    uint32_t MemoryMapAddress;
} __attribute__((packed));

struct MemoryMapEntry {
    uint32_t Size;  ///< Excludes this field itself
    uint64_t Address;
    uint64_t Length;
    uint32_t Type;
} __attribute__((packed));

struct ModuleEntry {
    uint32_t Start;
    uint32_t End;
    uint32_t CommandLine;
    uint32_t Reserved;
} __attribute__((packed));

}  // namespace V1

// --- Cached view ------------------------------------------------------------

static Standard s_Standard = Standard::NONE;
static const char* s_CommandLine = nullptr;
static const char* s_BootLoaderName = nullptr;
static uintptr_t s_AcpiRsdp = 0;

static const uint8_t* s_MemoryMapEntries = nullptr;
static uint32_t s_MemoryMapEntrySize = 0;
static unsigned s_MemoryRegionCount = 0;

static const void* s_Modules[16];
static unsigned s_ModuleCount = 0;

static Framebuffer s_Framebuffer;
static bool s_HasFramebuffer = false;

static void ParseV2(uintptr_t InInfo) {
    const auto* header = reinterpret_cast<const V2::InfoHeader*>(InInfo);
    const uintptr_t end = InInfo + header->TotalSize;

    for (const V2::Tag* tag = reinterpret_cast<const V2::Tag*>(InInfo + sizeof(V2::InfoHeader));
         reinterpret_cast<uintptr_t>(tag) < end && tag->Type != V2::END; tag = tag->Next()) {
        switch (tag->Type) {
            case V2::CMDLINE:
                s_CommandLine = static_cast<const V2::StringTag*>(tag)->String;
                break;

            case V2::BOOT_LOADER_NAME:
                s_BootLoaderName = static_cast<const V2::StringTag*>(tag)->String;
                break;

            case V2::MODULE:
                if (s_ModuleCount < ArraySize(s_Modules)) {
                    s_Modules[s_ModuleCount++] = tag;
                }
                break;

            case V2::MEMORY_MAP: {
                const auto* map = static_cast<const V2::MemoryMapTag*>(tag);
                s_MemoryMapEntries = map->Entries;
                s_MemoryMapEntrySize = map->EntrySize;
                s_MemoryRegionCount = (map->Size - sizeof(V2::MemoryMapTag)) / map->EntrySize;
                break;
            }

            case V2::FRAMEBUFFER: {
                const auto* framebuffer = static_cast<const V2::FramebufferTag*>(tag);
                s_Framebuffer = {framebuffer->Address,       framebuffer->Pitch,
                                 framebuffer->Width,         framebuffer->Height,
                                 framebuffer->BitsPerPixel, framebuffer->FramebufferType};
                s_HasFramebuffer = true;
                break;
            }

            case V2::ACPI_OLD:
            case V2::ACPI_NEW:
                // The loader hands over a copy of the RSDP, not a pointer to the
                // original. Prefer the 2.0 one when both are present.
                if (s_AcpiRsdp == 0 || tag->Type == V2::ACPI_NEW) {
                    s_AcpiRsdp = reinterpret_cast<uintptr_t>(static_cast<const V2::AcpiTag*>(tag)->Rsdp);
                }
                break;

            default:
                break;
        }
    }
}

static void ParseV1(uintptr_t InInfo) {
    const auto* info = reinterpret_cast<const V1::Info*>(InInfo);

    if ((info->Flags & V1::CMDLINE) != 0) {
        s_CommandLine = reinterpret_cast<const char*>(static_cast<uintptr_t>(info->CommandLine));
    }
    if ((info->Flags & V1::BOOT_LOADER_NAME) != 0) {
        s_BootLoaderName = reinterpret_cast<const char*>(static_cast<uintptr_t>(info->Syms[3]));
    }
    if ((info->Flags & V1::MEMORY_MAP) != 0 && info->MemoryMapLength >= sizeof(V1::MemoryMapEntry)) {
        // Multiboot1 entries are self-describing; assume they are uniform, which
        // every loader in practice makes them.
        const auto* first = reinterpret_cast<const V1::MemoryMapEntry*>(
            static_cast<uintptr_t>(info->MemoryMapAddress));
        s_MemoryMapEntrySize = first->Size + sizeof(uint32_t);
        s_MemoryMapEntries = reinterpret_cast<const uint8_t*>(first);
        s_MemoryRegionCount = info->MemoryMapLength / s_MemoryMapEntrySize;
    }
    if ((info->Flags & V1::MODULES) != 0) {
        const auto* modules =
            reinterpret_cast<const V1::ModuleEntry*>(static_cast<uintptr_t>(info->ModuleAddress));
        for (uint32_t i = 0; i < info->ModuleCount && s_ModuleCount < ArraySize(s_Modules); i++) {
            s_Modules[s_ModuleCount++] = &modules[i];
        }
    }
}

bool Initialize() {
    const uintptr_t info = MultibootInfo;

    if (MultibootMagic == LOADER_MAGIC_V2) {
        s_Standard = Standard::V2;
        ParseV2(info);
    } else if (MultibootMagic == LOADER_MAGIC_V1) {
        s_Standard = Standard::V1;
        ParseV1(info);
    } else {
        s_Standard = Standard::NONE;
        return false;
    }
    return true;
}

Standard GetStandard() {
    return s_Standard;
}

const char* GetCommandLine() {
    return s_CommandLine != nullptr ? s_CommandLine : "";
}

const char* GetBootLoaderName() {
    return s_BootLoaderName != nullptr ? s_BootLoaderName : "unknown";
}

uintptr_t GetAcpiRsdp() {
    return s_AcpiRsdp;
}

unsigned GetMemoryRegionCount() {
    return s_MemoryRegionCount;
}

bool GetMemoryRegion(unsigned InIndex, MemoryRegion& OutRegion) {
    if (InIndex >= s_MemoryRegionCount || s_MemoryMapEntries == nullptr) {
        return false;
    }

    const uint8_t* entry = s_MemoryMapEntries + InIndex * s_MemoryMapEntrySize;
    if (s_Standard == Standard::V2) {
        const auto* record = reinterpret_cast<const V2::MemoryMapEntry*>(entry);
        OutRegion = {record->Address, record->Length, static_cast<MemoryType>(record->Type)};
    } else {
        const auto* record = reinterpret_cast<const V1::MemoryMapEntry*>(entry);
        OutRegion = {record->Address, record->Length, static_cast<MemoryType>(record->Type)};
    }
    return true;
}

uint64_t GetAvailableMemory() {
    uint64_t total = 0;
    MemoryRegion region;
    for (unsigned i = 0; GetMemoryRegion(i, region); i++) {
        if (region.Type == MemoryType::AVAILABLE) {
            total += region.Length;
        }
    }
    return total;
}

unsigned GetModuleCount() {
    return s_ModuleCount;
}

bool GetModule(unsigned InIndex, Module& OutModule) {
    if (InIndex >= s_ModuleCount) {
        return false;
    }

    if (s_Standard == Standard::V2) {
        const auto* tag = static_cast<const V2::ModuleTag*>(s_Modules[InIndex]);
        OutModule = {tag->Start, tag->End, tag->CommandLine};
    } else {
        const auto* entry = static_cast<const V1::ModuleEntry*>(s_Modules[InIndex]);
        OutModule = {entry->Start, entry->End,
                     reinterpret_cast<const char*>(static_cast<uintptr_t>(entry->CommandLine))};
    }
    return true;
}

const Framebuffer* GetFramebuffer() {
    return s_HasFramebuffer ? &s_Framebuffer : nullptr;
}

void Dump() {
    const char* standard = s_Standard == Standard::V2   ? "Multiboot2"
                           : s_Standard == Standard::V1 ? "Multiboot1"
                                                        : "none";
    DBG << "Boot: " << standard << " via " << GetBootLoaderName() << EndLine;

    if (s_CommandLine != nullptr && s_CommandLine[0] != '\0') {
        DBG << "  cmdline: " << s_CommandLine << EndLine;
    }
    DBG << "  usable memory: " << Dec << (GetAvailableMemory() / MIB) << " MiB in "
        << s_MemoryRegionCount << " regions" << EndLine;
    if (s_AcpiRsdp != 0) {
        DBG << "  ACPI RSDP: " << reinterpret_cast<void*>(s_AcpiRsdp) << EndLine;
    }

    Module module;
    for (unsigned i = 0; GetModule(i, module); i++) {
        DBG << "  module " << i << ": " << reinterpret_cast<void*>(module.Start) << " + " << Dec
            << module.Size() << " bytes" << EndLine;
    }
}

}  // namespace Multiboot
