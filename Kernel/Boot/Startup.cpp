#include "Boot/Startup.h"
#include "Boot/Multiboot.h"
#include "Arch/Acpi.h"
#include "Arch/Apic.h"
#include "Arch/Cpu.h"
#include "Arch/Idt.h"
#include "Arch/Pic.h"
#include "Compiler/Libc.h"
#include "Debug/Output.h"
#include "Interrupt/Handlers.h"

/// Whoever gets here first is the bootstrap processor; the application
/// processors only arrive much later, after it has explicitly started them.
static bool s_IsBootstrapProcessor = true;

extern "C" [[noreturn]] void KernelInit() {
    if (s_IsBootstrapProcessor) {
        s_IsBootstrapProcessor = false;

        // Global constructors first, so every later step can use the output
        // streams and the other global objects.
        Csu::RunInitializers();

        Multiboot::Initialize();

        // An IDT before anything else means a fault during setup produces a
        // readable message instead of a triple fault reboot loop.
        Interrupt::InstallHandlers();

        // The legacy PICs still raise IRQs on the exception vectors until they
        // are remapped and masked.
        Pic::Initialize();

        if (!Acpi::Initialize(Multiboot::GetAcpiRsdp())) {
            DBG << "ACPI unavailable, cannot continue" << EndLine;
            Cpu::Die();
        }
        if (!Apic::Initialize()) {
            DBG << "APIC initialisation failed, cannot continue" << EndLine;
            Cpu::Die();
        }

        Cpu::Initialize();

        Main();
        MainAp();

        DBG_VERBOSE << "Core " << Cpu::GetId() << " (BSP) shutting down" << EndLine;
    } else {
        Idt::Load();
        Cpu::Initialize();

        MainAp();

        DBG_VERBOSE << "Core " << Cpu::GetId() << " (AP) shutting down" << EndLine;
    }

    Cpu::Shutdown();

    if (Cpu::CountOnline() == 0) {
        Csu::RunFinalizers();
    }

    Cpu::Die();
}
