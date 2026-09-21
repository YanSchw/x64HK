#include "Arch/IoApic.h"
#include "Arch/Apic.h"
#include "Arch/Cpu.h"

namespace IoApic {

// Register access is indirect: write the register number to IOREGSEL, then read
// or write IOWIN. Both are memory mapped, 16 bytes apart.
static volatile Index* s_RegisterSelect = nullptr;
static volatile Register* s_RegisterWindow = nullptr;

/// A single I/O APIC covers 24 pins, which is all a legacy PC needs.
constexpr uint8_t PIN_COUNT = 24;

static Register ReadRegister(Index InIndex) {
    *s_RegisterSelect = InIndex;
    return *s_RegisterWindow;
}

static void WriteRegister(Index InIndex, Register InValue) {
    *s_RegisterSelect = InIndex;
    *s_RegisterWindow = InValue;
}

static RedirectionTableEntry ReadEntry(uint8_t InPin) {
    const Index low = REDIRECTION_TABLE + 2 * InPin;
    return RedirectionTableEntry(ReadRegister(low), ReadRegister(low + 1));
}

static void WriteEntry(uint8_t InPin, RedirectionTableEntry InEntry) {
    const Index low = REDIRECTION_TABLE + 2 * InPin;
    // Mask bit first: writing the low half unmasks, so the high half (which
    // picks the target core) has to be valid before that happens.
    WriteRegister(low + 1, InEntry.ValueHigh);
    WriteRegister(low, InEntry.ValueLow);
}

void Initialize() {
    const uintptr_t base = Apic::GetIoApicAddress();
    s_RegisterSelect = reinterpret_cast<volatile Index*>(base);
    s_RegisterWindow = reinterpret_cast<volatile Register*>(base + 0x10);

    Identification identification(ReadRegister(IDENTIFICATION));
    identification.Id = Apic::GetIoApicId();
    WriteRegister(IDENTIFICATION, identification.Value);

    // Every core is a candidate; the hardware picks the least busy one. QEMU
    // ignores this and always delivers to the bootstrap processor.
    const uint64_t allCores = (1ULL << Cpu::Count()) - 1;

    for (uint8_t pin = 0; pin < PIN_COUNT; pin++) {
        RedirectionTableEntry entry(0, 0);
        entry.Vector = ToUnderlying(Cpu::Interrupt::Vector::PANIC);
        entry.Delivery = DeliveryMode::LOWEST_PRIORITY;
        entry.Destination = DestinationMode::LOGICAL;
        entry.Trigger = TriggerMode::EDGE;
        entry.Mask = InterruptMask::MASKED;
        entry.DestinationId = allCores;
        WriteEntry(pin, entry);
    }
}

void Configure(uint8_t InPin, Cpu::Interrupt::Vector InVector, TriggerMode InTriggerMode,
               Polarity InPolarity) {
    if (InPin >= PIN_COUNT) {
        return;
    }
    RedirectionTableEntry entry = ReadEntry(InPin);
    entry.Vector = ToUnderlying(InVector);
    entry.Trigger = InTriggerMode;
    entry.Polarity = InPolarity;
    WriteEntry(InPin, entry);
}

void Allow(uint8_t InPin) {
    if (InPin >= PIN_COUNT) {
        return;
    }
    RedirectionTableEntry entry = ReadEntry(InPin);
    entry.Mask = InterruptMask::UNMASKED;
    WriteEntry(InPin, entry);
}

void Forbid(uint8_t InPin) {
    if (InPin >= PIN_COUNT) {
        return;
    }
    RedirectionTableEntry entry = ReadEntry(InPin);
    entry.Mask = InterruptMask::MASKED;
    WriteEntry(InPin, entry);
}

bool IsAllowed(uint8_t InPin) {
    return InPin < PIN_COUNT && ReadEntry(InPin).Mask == InterruptMask::UNMASKED;
}

}  // namespace IoApic
