#pragma once
#include "Types.h"

// Advanced Configuration and Power Interface.
//
// Only the description tables are of interest here: the MADT is the one source
// of truth for how many cores exist, what their LAPIC IDs are and where the I/O
// APIC lives.
namespace Acpi {

/// Root System Description Pointer -- the entry point into everything else.
struct Rsdp {
    char Signature[8];  ///< "RSD PTR "
    uint8_t Checksum;
    char OemId[6];
    uint8_t Revision;  ///< 0 = ACPI 1.0 (RSDT only), >= 2 has an XSDT
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} __attribute__((packed));

/// Header shared by every description table.
struct TableHeader {
    uint32_t Signature;
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OemId[6];
    char OemTableId[8];
    uint32_t OemRevision;
    uint32_t CreatorId;
    uint32_t CreatorRevision;

    void* End() { return reinterpret_cast<uint8_t*>(this) + Length; }
} __attribute__((packed));

struct Rsdt : TableHeader {
    uint32_t Entries[];
} __attribute__((packed));

/// Same as the RSDT but with 64-bit pointers. Takes precedence when present.
struct Xsdt : TableHeader {
    uint64_t Entries[];
} __attribute__((packed));

struct SubHeader {
    uint8_t Type;
    uint8_t Length;

    SubHeader* Next() { return reinterpret_cast<SubHeader*>(reinterpret_cast<uint8_t*>(this) + Length); }
} __attribute__((packed));

/// Multiple APIC Description Table: a header followed by variable length
/// records describing every interrupt controller in the system.
struct Madt : TableHeader {
    uint32_t LocalApicAddress;
    uint32_t FlagsPcAtCompatible : 1, FlagsReserved : 31;

    SubHeader* First() { return reinterpret_cast<SubHeader*>(reinterpret_cast<uint8_t*>(this) + sizeof(Madt)); }
} __attribute__((packed));

namespace MadtEntry {

enum Type : uint8_t {
    LOCAL_APIC = 0,
    IO_APIC = 1,
    INTERRUPT_SOURCE_OVERRIDE = 2,
    LOCAL_APIC_ADDRESS_OVERRIDE = 5,
};

struct LocalApic : SubHeader {
    uint8_t AcpiProcessorId;
    uint8_t ApicId;
    uint32_t FlagsEnabled : 1, FlagsReserved : 31;
} __attribute__((packed));

struct IoApic : SubHeader {
    uint8_t IoApicId;
    uint8_t Reserved;
    uint32_t IoApicAddress;
    uint32_t GlobalSystemInterruptBase;
} __attribute__((packed));

/// Records where firmware rewired a legacy ISA IRQ to a different I/O APIC pin.
struct InterruptSourceOverride : SubHeader {
    uint8_t Bus;
    uint8_t Source;
    uint32_t GlobalSystemInterrupt;
    uint16_t FlagsPolarity : 2, FlagsTriggerMode : 2, FlagsReserved : 12;
} __attribute__((packed));

struct LocalApicAddressOverride : SubHeader {
    uint16_t Reserved;
    uint64_t LocalApicAddress;
} __attribute__((packed));

}  // namespace MadtEntry

/// Locates the description tables. InRsdpHint is the pointer handed over by the
/// boot loader; pass 0 to fall back to scanning the EBDA and BIOS ROM.
bool Initialize(uintptr_t InRsdpHint);

unsigned Count();
TableHeader* Get(unsigned InIndex);

/// Looks a table up by its four character signature, e.g. Get('A','P','I','C').
TableHeader* Get(char InA, char InB, char InC, char InD);

int Revision();

}  // namespace Acpi
