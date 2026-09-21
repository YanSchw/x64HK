#include "Arch/Acpi.h"
#include "Debug/Output.h"

// Reading the EBDA pointer means dereferencing 0x40e, which sits in the page the
// compiler assumes is never mapped.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

namespace Acpi {

static const Rsdp* s_Rsdp = nullptr;
static Rsdt* s_Rsdt = nullptr;
static Xsdt* s_Xsdt = nullptr;

constexpr const char* RSDP_SIGNATURE = "RSD PTR ";

/// ACPI tables are valid when all their bytes sum to zero modulo 256.
static bool IsChecksumValid(const void* InAddress, unsigned InLength) {
    const uint8_t* bytes = static_cast<const uint8_t*>(InAddress);
    uint8_t sum = 0;
    for (unsigned i = 0; i < InLength; i++) {
        sum += bytes[i];
    }
    return sum == 0;
}

static bool IsValidRsdp(const Rsdp* InCandidate) {
    // Revision 0 only covers the first 20 bytes; later revisions add a second
    // checksum over the whole structure.
    if (InCandidate->Revision == 0) {
        return IsChecksumValid(InCandidate, 20);
    }
    return InCandidate->Length > 20 && IsChecksumValid(InCandidate, InCandidate->Length);
}

/// The RSDP is 16-byte aligned, so only every second 64-bit word can hold the
/// signature.
static const Rsdp* Scan(uintptr_t InStart, size_t InLength) {
    const uint64_t wanted = *reinterpret_cast<const uint64_t*>(RSDP_SIGNATURE);
    for (size_t offset = 0; offset + sizeof(Rsdp) <= InLength; offset += 16) {
        const uint64_t* candidate = reinterpret_cast<const uint64_t*>(InStart + offset);
        if (*candidate == wanted) {
            const Rsdp* rsdp = reinterpret_cast<const Rsdp*>(candidate);
            if (IsValidRsdp(rsdp)) {
                return rsdp;
            }
        }
    }
    return nullptr;
}

bool Initialize(uintptr_t InRsdpHint) {
    if (InRsdpHint != 0) {
        const Rsdp* candidate = reinterpret_cast<const Rsdp*>(InRsdpHint);
        if (IsValidRsdp(candidate)) {
            s_Rsdp = candidate;
        }
    }

    if (s_Rsdp == nullptr) {
        // The first kilobyte of the Extended BIOS Data Area, whose segment is
        // stored as a word at 0x40e in the BIOS data area.
        const uintptr_t ebda = static_cast<uintptr_t>(*reinterpret_cast<uint16_t*>(0x40e)) << 4;
        s_Rsdp = Scan(ebda, 1024);
    }
    if (s_Rsdp == nullptr) {
        // Otherwise the read-only BIOS area.
        s_Rsdp = Scan(0xe0000, 0x20000);
    }
    if (s_Rsdp == nullptr) {
        DBG << "ACPI: no RSDP found" << EndLine;
        return false;
    }

    s_Rsdt = reinterpret_cast<Rsdt*>(static_cast<uintptr_t>(s_Rsdp->RsdtAddress));
    // "An ACPI-compatible OS must use the XSDT if present."
    if (s_Rsdp->Revision != 0 && s_Rsdp->Length >= 36) {
        s_Xsdt = reinterpret_cast<Xsdt*>(s_Rsdp->XsdtAddress);
    }

    DBG_VERBOSE << "ACPI revision " << static_cast<unsigned>(s_Rsdp->Revision) << ", " << Count()
                << " tables" << EndLine;
    return true;
}

unsigned Count() {
    if (s_Xsdt != nullptr) {
        return (s_Xsdt->Length - sizeof(TableHeader)) / sizeof(uint64_t);
    }
    if (s_Rsdt != nullptr) {
        return (s_Rsdt->Length - sizeof(TableHeader)) / sizeof(uint32_t);
    }
    return 0;
}

TableHeader* Get(unsigned InIndex) {
    if (InIndex >= Count()) {
        return nullptr;
    }

    TableHeader* entry = nullptr;
    if (s_Xsdt != nullptr) {
        entry = reinterpret_cast<TableHeader*>(s_Xsdt->Entries[InIndex]);
    } else if (s_Rsdt != nullptr) {
        entry = reinterpret_cast<TableHeader*>(static_cast<uintptr_t>(s_Rsdt->Entries[InIndex]));
    }

    return entry != nullptr && IsChecksumValid(entry, entry->Length) ? entry : nullptr;
}

TableHeader* Get(char InA, char InB, char InC, char InD) {
    const uint32_t wanted = static_cast<uint32_t>(InA) | (static_cast<uint32_t>(InB) << 8) |
                            (static_cast<uint32_t>(InC) << 16) | (static_cast<uint32_t>(InD) << 24);

    const unsigned count = Count();
    for (unsigned i = 0; i < count; i++) {
        TableHeader* entry = Get(i);
        if (entry != nullptr && entry->Signature == wanted) {
            return entry;
        }
    }
    return nullptr;
}

int Revision() {
    return s_Rsdp != nullptr ? static_cast<int>(s_Rsdp->Revision) : -1;
}

}  // namespace Acpi

#pragma GCC diagnostic pop
