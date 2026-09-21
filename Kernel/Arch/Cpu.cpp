#include "Arch/Cpu.h"
#include "Arch/Apic.h"
#include "Arch/Gdt.h"
#include "Arch/LocalApic.h"
#include "Debug/Assert.h"

// Boot stacks, handed out by Boot/Entry.asm before any C++ runs.
//
// Entry.asm grabs its slice with a 32-bit `lock xadd` on CoreStackPointer. That
// is a 32-bit operation on a 64-bit pointer, which is only safe because the
// kernel is linked below 4 GiB, so the upper half of the pointer is always zero.
alignas(16) static uint8_t s_CoreStacks[Config::MAX_CORES * Config::BOOT_STACK_SIZE];

extern "C" {
// `extern` is load bearing here: a namespace scope const would otherwise have
// internal linkage and Entry.asm could not see it.
extern const uint32_t CoreStackSize;
const uint32_t CoreStackSize = Config::BOOT_STACK_SIZE;
uint8_t* CoreStackPointer = s_CoreStacks;
}

namespace Cpu {

/// LAPIC IDs are sparse and firmware assigned; this maps them to a dense index.
static volatile unsigned s_CoreIdByLapicId[256];
static unsigned s_CoreCount = 0;
static unsigned s_OnlineCount = 0;
static bool s_CoreOnline[Config::MAX_CORES];

unsigned GetId() {
    return s_CoreIdByLapicId[LocalApic::GetId()];
}

void Initialize() {
    // The first core through builds the lookup table. Application processors are
    // only started once the bootstrap processor has finished, so no lock is
    // needed here -- the counter just identifies who is first.
    if (__atomic_fetch_add(&s_OnlineCount, 1, __ATOMIC_ACQ_REL) == 0) {
        for (unsigned core = 0; core < Config::MAX_CORES; core++) {
            const uint8_t lapicId = Apic::GetLocalApicId(core);
            if (lapicId != Apic::INVALID_ID) {
                s_CoreIdByLapicId[lapicId] = core;
                s_CoreCount++;
            }
        }
    }

    const unsigned id = GetId();
    LocalApic::Initialize(Apic::GetLogicalApicId(id));
    Gdt::LoadTaskRegister(id);
    s_CoreOnline[id] = true;
}

void Shutdown() {
    s_CoreOnline[GetId()] = false;
    __atomic_fetch_sub(&s_OnlineCount, 1, __ATOMIC_ACQ_REL);
}

unsigned Count() {
    return s_CoreCount;
}

unsigned CountOnline() {
    return __atomic_load_n(&s_OnlineCount, __ATOMIC_ACQUIRE);
}

bool IsOnline(unsigned InCoreId) {
    return InCoreId < Config::MAX_CORES && s_CoreOnline[InCoreId];
}

}  // namespace Cpu
