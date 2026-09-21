#include "Config.h"
#include "Boot/Multiboot.h"
#include "Boot/SmpBoot.h"
#include "Boot/Startup.h"
#include "App/CounterThread.h"
#include "App/KeyboardThread.h"
#include "App/MelodyThread.h"
#include "Arch/Cga.h"
#include "Arch/Cpu.h"
#include "Arch/IoApic.h"
#include "Arch/LocalApic.h"
#include "Debug/Assert.h"
#include "Debug/CopyStream.h"
#include "Debug/Output.h"
#include "Device/Ps2Controller.h"
#include "Device/SerialStream.h"
#include "Device/TextStream.h"
#include "Interrupt/Guard.h"
#include "Memory/Heap.h"

// Screen layout: the shared Vault window owns the top four rows, the rest is a
// two-by-four grid of per-core debug windows. One window per core is what keeps
// concurrent output from interleaving mid-line.
static TextStream s_DebugWindow[Config::MAX_CORES]{
    {0, 39, 4, 9},   {41, 80, 4, 9},   {0, 39, 9, 14},  {41, 80, 9, 14},
    {0, 39, 14, 19}, {41, 80, 14, 19}, {0, 39, 19, 24}, {41, 80, 19, 24},
};

static SerialStream s_Serial;

/// Core 0's output also goes to the serial line, which is the only channel that
/// survives a screen full of scrolling and can be captured from the host.
static CopyStream s_BootstrapOutput(&s_DebugWindow[0], &s_Serial);

static CounterThread s_CounterA("A:", Cga::Attribute(Cga::Color::WHITE, Cga::Color::BLUE), 1);
static CounterThread s_CounterB("B:", Cga::Attribute(Cga::Color::WHITE, Cga::Color::GREEN), 2);
static KeyboardThread s_Keyboard;
static MelodyThread s_Melody;

static void RegisterDebugStreams() {
    Debug::SetStream(0, &s_BootstrapOutput);
    for (unsigned core = 1; core < Config::MAX_CORES; core++) {
        Debug::SetStream(core, &s_DebugWindow[core]);
    }
}

extern "C" int Main() {
    RegisterDebugStreams();
    Heap::Initialize();

    DBG << "x64HK on " << Dec << Cpu::Count() << " cores" << EndLine;
    Multiboot::Dump();

    IoApic::Initialize();
    Ps2Controller::Initialize();

    const bool timerReady = LocalApic::Timer::Setup(Config::SCHEDULER_TICK_MS * 1000);
    ASSERT(timerReady);

    {
        Guarded guard = Guard::Enter();
        Scheduler& scheduler = guard.Vault().Scheduler;
        scheduler.Ready(&s_CounterA);
        scheduler.Ready(&s_CounterB);
        scheduler.Ready(&s_Keyboard);
        scheduler.Ready(&s_Melody);
    }

    // Interrupts must still be off here: a Startup-IPI that gets interrupted
    // halfway can leave the LAPIC waiting for an end-of-interrupt that never
    // comes, which silently kills every later device interrupt.
    SmpBoot::Boot();

    return 0;
}

extern "C" int MainAp() {
    DBG_VERBOSE << "Core " << Cpu::GetId() << " (LAPIC " << static_cast<unsigned>(LocalApic::GetId()) << ") entering scheduler"
                << EndLine;

    LocalApic::Timer::Activate();

    // Schedule() never returns; the first thread releases the Guard from its
    // kickoff, which is what hands the critical section over to thread level.
    Guarded guard = Guard::Enter();
    Cpu::Interrupt::Enable();
    guard.Vault().Scheduler.Schedule();
}
