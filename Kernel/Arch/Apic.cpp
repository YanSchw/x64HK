#include "Arch/Apic.h"
#include "Arch/Acpi.h"
#include "Arch/Cpu.h"
#include "Arch/IoPort.h"
#include "Arch/LocalApicRegisters.h"
#include "Config.h"
#include "Debug/Output.h"

namespace Apic {

static struct {
    uint32_t Id;
    uintptr_t Address;
    uint32_t InterruptBase;
} s_IoApic;

/// ISA IRQ -> global system interrupt, identity mapped unless the MADT says so.
static uint8_t s_PinMap[16];
static uint8_t s_LocalApicId[Config::MAX_CORES];
static unsigned s_LocalApicCount = 0;

bool Initialize() {
    Acpi::Madt* madt = static_cast<Acpi::Madt*>(Acpi::Get('A', 'P', 'I', 'C'));
    if (madt == nullptr) {
        DBG << "APIC: no MADT in ACPI" << EndLine;
        return false;
    }

    LocalApic::s_BaseAddress = static_cast<uintptr_t>(madt->LocalApicAddress);

    if (madt->FlagsPcAtCompatible != 0) {
        // The chipset came up in legacy PIC mode. The Interrupt Mode Control
        // Register only exists on such hardware and switches the interrupt lines
        // over to the APICs.
        IoPort(0x22).OutB(0x70);
        IoPort(0x23).OutB(0x01);
    }

    for (unsigned i = 0; i < ArraySize(s_PinMap); i++) {
        s_PinMap[i] = static_cast<uint8_t>(i);
    }
    for (unsigned i = 0; i < Config::MAX_CORES; i++) {
        s_LocalApicId[i] = INVALID_ID;
    }

    for (Acpi::SubHeader* entry = madt->First(); entry < madt->End(); entry = entry->Next()) {
        switch (entry->Type) {
            case Acpi::MadtEntry::LOCAL_APIC: {
                auto* record = static_cast<Acpi::MadtEntry::LocalApic*>(entry);
                if (record->FlagsEnabled == 0 || record->ApicId == INVALID_ID) {
                    break;
                }
                if (s_LocalApicCount >= Config::MAX_CORES) {
                    DBG << "APIC: more cores than Config::MAX_CORES, ignoring the rest" << EndLine;
                    break;
                }
                s_LocalApicId[s_LocalApicCount++] = record->ApicId;
                break;
            }

            case Acpi::MadtEntry::IO_APIC: {
                auto* record = static_cast<Acpi::MadtEntry::IoApic*>(entry);
                // Only the I/O APIC that owns the legacy IRQ range is used.
                if (record->GlobalSystemInterruptBase > 23) {
                    break;
                }
                s_IoApic.Id = record->IoApicId;
                s_IoApic.Address = static_cast<uintptr_t>(record->IoApicAddress);
                s_IoApic.InterruptBase = record->GlobalSystemInterruptBase;
                break;
            }

            case Acpi::MadtEntry::INTERRUPT_SOURCE_OVERRIDE: {
                auto* record = static_cast<Acpi::MadtEntry::InterruptSourceOverride*>(entry);
                // Bus 0 is ISA; the spec defines no other value here.
                if (record->Bus == 0 && record->Source < ArraySize(s_PinMap)) {
                    s_PinMap[record->Source] = static_cast<uint8_t>(record->GlobalSystemInterrupt);
                }
                break;
            }

            case Acpi::MadtEntry::LOCAL_APIC_ADDRESS_OVERRIDE: {
                auto* record = static_cast<Acpi::MadtEntry::LocalApicAddressOverride*>(entry);
                LocalApic::s_BaseAddress = static_cast<uintptr_t>(record->LocalApicAddress);
                break;
            }

            default:
                break;
        }
    }

    DBG_VERBOSE << "APIC: " << s_LocalApicCount << " local APICs, I/O APIC at "
                << reinterpret_cast<void*>(s_IoApic.Address) << EndLine;
    return s_LocalApicCount > 0;
}

uintptr_t GetIoApicAddress() {
    return s_IoApic.Address;
}

uint8_t GetIoApicId() {
    return static_cast<uint8_t>(s_IoApic.Id);
}

uint8_t GetIoApicPin(Device InDevice) {
    return s_PinMap[ToUnderlying(InDevice)];
}

uint8_t GetLocalApicId(unsigned InCoreId) {
    return InCoreId < Config::MAX_CORES ? s_LocalApicId[InCoreId] : INVALID_ID;
}

uint8_t GetLogicalApicId(unsigned InCoreId) {
    // Flat logical mode packs one core per bit, which caps this scheme at 8.
    static_assert(Config::MAX_CORES <= 8, "Flat logical APIC addressing only has 8 bits");
    return InCoreId < Config::MAX_CORES ? static_cast<uint8_t>(1U << InCoreId) : 0;
}

}  // namespace Apic
