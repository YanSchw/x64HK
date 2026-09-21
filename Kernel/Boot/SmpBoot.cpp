#include "Boot/SmpBoot.h"
#include "Config.h"
#include "Arch/Cpu.h"
#include "Arch/Gdt.h"
#include "Arch/LocalApic.h"
#include "Arch/Pit.h"
#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Lib/String.h"

// Provided by the linker script around the .ap_trampoline section.
extern "C" uint8_t ___AP_TRAMPOLINE_START___;
extern "C" uint8_t ___AP_TRAMPOLINE_END___;

// Placeholders inside that section, patched below.
extern "C" uint8_t ApTrampolineGdt;
extern "C" uint8_t ApTrampolineGdtPointer;

namespace SmpBoot {

// A Startup-IPI carries a page number, so the trampoline has to sit on a 4 KiB
// boundary below 1 MiB.
static_assert((Config::AP_TRAMPOLINE_ADDRESS & 0xfff) == 0, "AP trampoline must be page aligned");
static_assert(Config::AP_TRAMPOLINE_ADDRESS < 1 * MIB, "AP trampoline must live below 1 MiB");

/// Blueprint for the GDT the trampoline runs on: flat 32-bit code and data, just
/// enough to survive until the shared boot path reloads the real one.
constinit static Gdt::SegmentDescriptor s_TrampolineGdt[] = {
    Gdt::SegmentDescriptor::Null(),
    Gdt::SegmentDescriptor::Segment(0, UINT32_MAX, true, 0, Gdt::Size::BIT32),
    Gdt::SegmentDescriptor::Segment(0, UINT32_MAX, false, 0, Gdt::Size::BIT32),
};

static void RelocateTrampoline() {
    const size_t length = &___AP_TRAMPOLINE_END___ - &___AP_TRAMPOLINE_START___;
    void* target = reinterpret_cast<void*>(Config::AP_TRAMPOLINE_ADDRESS);

    memcpy(target, &___AP_TRAMPOLINE_START___, length);

    // The descriptors have to describe where the copy ended up, not where it was
    // linked, so both the table and its pointer are rewritten in place.
    const uintptr_t gdtOffset = &ApTrampolineGdt - &___AP_TRAMPOLINE_START___;
    const uintptr_t pointerOffset = &ApTrampolineGdtPointer - &___AP_TRAMPOLINE_START___;

    void* relocatedGdt = reinterpret_cast<void*>(Config::AP_TRAMPOLINE_ADDRESS + gdtOffset);
    memcpy(relocatedGdt, s_TrampolineGdt, sizeof(s_TrampolineGdt));

    auto* relocatedPointer =
        reinterpret_cast<Gdt::Pointer*>(Config::AP_TRAMPOLINE_ADDRESS + pointerOffset);
    relocatedPointer->Set(relocatedGdt, ArraySize(s_TrampolineGdt));
}

void Boot() {
    ASSERT(!Cpu::Interrupt::IsEnabled());

    if (Cpu::Count() <= 1) {
        DBG_VERBOSE << "Single core system, no APs to start" << EndLine;
        return;
    }

    RelocateTrampoline();

    const uint8_t startupVector = Config::AP_TRAMPOLINE_ADDRESS >> 12;

    // The INIT/SIPI/SIPI dance from the Intel MP spec: INIT resets the target
    // into wait-for-SIPI, and the second SIPI is a belt-and-braces retry for
    // chips that drop the first one.
    LocalApic::Ipi::SendInit();
    Pit::Delay(10'000);

    LocalApic::Ipi::SendStartup(startupVector);
    Pit::Delay(200);

    LocalApic::Ipi::SendStartup(startupVector);

    DBG_VERBOSE << "Startup IPIs sent for " << (Cpu::Count() - 1) << " application processors"
                << EndLine;
}

}  // namespace SmpBoot
