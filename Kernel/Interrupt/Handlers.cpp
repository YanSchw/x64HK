#include "Interrupt/Handlers.h"
#include "Interrupt/Epilogue.h"
#include "Interrupt/Guard.h"
#include "Arch/ControlRegister.h"
#include "Arch/Cpu.h"
#include "Arch/Gdt.h"
#include "Arch/Idt.h"
#include "Arch/LocalApic.h"
#include "Device/Ps2Controller.h"
#include "Memory/KernelStack.h"
#include "Debug/Output.h"
#include "Debug/Panic.h"

// Every handler here carries the `interrupt` attribute: the compiler then saves
// and restores all registers it touches and returns with iretq, which is what
// makes writing prologues in C++ safe. It is also why the whole kernel is built
// with -mgeneral-regs-only -- the attribute cannot preserve SSE state.

namespace Interrupt {

using Vector = Cpu::Interrupt::Vector;

static void PrintContext(const InterruptContext* InContext) {
    DBG << "  at " << Hex << InContext->Cs << ':' << InContext->Ip << "  stack " << InContext->Ss << ':'
        << InContext->Sp << "  flags " << InContext->Flags << Dec << EndLine;
}

// --- Exceptions -------------------------------------------------------------

[[gnu::interrupt]] static void HandleDivideError(InterruptContext* InContext) {
    DBG << "Divide error" << EndLine;
    PrintContext(InContext);
    PANIC("Divide error");
}

[[gnu::interrupt]] static void HandleInvalidOpcode(InterruptContext* InContext) {
    DBG << "Invalid opcode" << EndLine;
    PrintContext(InContext);
    PANIC("Invalid opcode");
}

[[gnu::interrupt]] static void HandleDoubleFault(InterruptContext* InContext, uint64_t InError) {
    // Reached when the CPU faults while delivering another fault. Runs on the
    // IST stack, because the usual one is often exactly what went wrong.
    DBG << "Double fault" << EndLine;
    PrintContext(InContext);
    PANIC("Double fault");
}

[[gnu::interrupt]] static void HandleInvalidTss(InterruptContext* InContext, uint64_t InError) {
    DBG << "Invalid TSS, selector index " << Dec << (InError >> 3) << EndLine;
    PrintContext(InContext);
    PANIC("Invalid TSS");
}

[[gnu::interrupt]] static void HandleSegmentNotPresent(InterruptContext* InContext, uint64_t InError) {
    DBG << "Segment not present, selector index " << Dec << (InError >> 3) << EndLine;
    PrintContext(InContext);
    PANIC("Segment not present");
}

[[gnu::interrupt]] static void HandleStackSegmentFault(InterruptContext* InContext, uint64_t InError) {
    DBG << "Stack segment fault, error " << Hex << InError << Dec << EndLine;
    PrintContext(InContext);
    PANIC("Stack segment fault");
}

[[gnu::interrupt]] static void HandleGeneralProtectionFault(InterruptContext* InContext,
                                                            uint64_t InError) {
    DBG << "General protection fault, error " << Hex << InError << Dec << EndLine;
    PrintContext(InContext);
    PANIC("General protection fault");
}

[[gnu::interrupt]] static void HandlePageFault(InterruptContext* InContext, uint64_t InError) {
    // CR2 holds the address that could not be translated.
    const uintptr_t address = Cpu::CR2::Read();

    if (KernelStack::IsGuardPage(address)) {
        // The stack is a mapping of its own now, so the thread object behind it
        // is still intact and safe to ask.
        const Thread* thread = Guard::UnsafeVault().Scheduler.Active();

        DBG << "Kernel stack overflow in '" << (thread != nullptr ? thread->Name() : "?") << "' at "
            << reinterpret_cast<void*>(address) << " on core " << Dec << Cpu::GetId() << EndLine;
        PrintContext(InContext);
        PANIC("Kernel stack overflow");
    }

    DBG << "Page fault at " << reinterpret_cast<void*>(address) << ' ' << PageFaultError(InError)
        << EndLine;
    PrintContext(InContext);
    PANIC("Page fault");
}

[[gnu::interrupt]] static void HandleNonMaskableInterrupt(InterruptContext* InContext) {
    DBG << "Non-maskable interrupt" << EndLine;
    PrintContext(InContext);
    PANIC("Non-maskable interrupt");
}

[[gnu::interrupt]] static void HandleMachineCheck(InterruptContext* InContext) {
    DBG << "Machine check" << EndLine;
    PrintContext(InContext);
    PANIC("Machine check");
}

// --- Fallbacks --------------------------------------------------------------

[[gnu::interrupt]] static void HandleUnexpected(InterruptContext* InContext) {
    // The in-service register is the only way back to the vector number from a
    // shared handler; it reads 0 for a software or CPU-generated interrupt.
    DBG << "Unexpected interrupt, LAPIC vector " << Dec << LocalApic::GetInServiceVector() << EndLine;
    PrintContext(InContext);
    PANIC("Unexpected interrupt");
}

[[gnu::interrupt]] static void HandleUnexpectedWithError(InterruptContext* InContext, uint64_t InError) {
    DBG << "Unexpected exception, error " << Hex << InError << Dec << EndLine;
    PrintContext(InContext);
    PANIC("Unexpected exception");
}

[[gnu::interrupt]] static void HandleSpurious(InterruptContext* InContext) {
    // Raised by the LAPIC itself when an interrupt is withdrawn between being
    // signalled and being delivered. It must NOT be acknowledged -- an EOI here
    // would clear somebody else's in-service bit.
}

// --- Devices ----------------------------------------------------------------

[[gnu::interrupt]] static void HandleTimer(InterruptContext* InContext) {
    LocalApic::EndOfInterrupt();
    Guard::Relay(Epilogues::Timer);
}

[[gnu::interrupt]] static void HandleKeyboard(InterruptContext* InContext) {
    // Read the scan codes out here, not in the epilogue: the I/O APIC entry is
    // level triggered, so an unread byte keeps the line asserted and the
    // interrupt fires again the instant this handler returns.
    Ps2Controller::DrainToQueue();
    LocalApic::EndOfInterrupt();
    Guard::Relay(Epilogues::Keyboard);
}

[[gnu::interrupt]] static void HandleAssassin(InterruptContext* InContext) {
    LocalApic::EndOfInterrupt();
    Guard::Relay(Epilogues::Assassin);
}

[[gnu::interrupt]] static void HandleWakeup(InterruptContext* InContext) {
    LocalApic::EndOfInterrupt();
    Guard::Relay(Epilogues::Wakeup);
}

void InstallHandlers() {
    // Default everything to the no-error-code fallback first.
    for (unsigned vector = 0; vector < Cpu::Interrupt::VECTOR_COUNT; vector++) {
        Idt::Set(static_cast<Vector>(vector), Idt::InterruptDescriptor::Returning(HandleUnexpected));
    }

    // These vectors push an error code, which shifts the stack frame and so
    // needs the other handler signature.
    constexpr Vector ERROR_CODE_VECTORS[] = {
        Vector::DOUBLE_FAULT,    Vector::INVALID_TSS,     Vector::SEGMENT_NOT_PRESENT,
        Vector::STACK_SEGMENT_FAULT, Vector::GENERAL_PROTECTION_FAULT, Vector::PAGE_FAULT,
        Vector::ALIGNMENT_CHECK, Vector::CONTROL_PROTECTION, Vector::SECURITY_EXCEPTION,
    };
    for (Vector vector : ERROR_CODE_VECTORS) {
        Idt::Set(vector, Idt::InterruptDescriptor::ReturningWithError(HandleUnexpectedWithError));
    }

    Idt::Set(Vector::DIVIDE_ERROR, Idt::InterruptDescriptor::Returning(HandleDivideError));
    Idt::Set(Vector::INVALID_OPCODE, Idt::InterruptDescriptor::Returning(HandleInvalidOpcode));
    Idt::Set(Vector::INVALID_TSS, Idt::InterruptDescriptor::ReturningWithError(HandleInvalidTss));
    Idt::Set(Vector::SEGMENT_NOT_PRESENT,
             Idt::InterruptDescriptor::ReturningWithError(HandleSegmentNotPresent));
    Idt::Set(Vector::STACK_SEGMENT_FAULT,
             Idt::InterruptDescriptor::ReturningWithError(HandleStackSegmentFault));
    Idt::Set(Vector::GENERAL_PROTECTION_FAULT,
             Idt::InterruptDescriptor::ReturningWithError(HandleGeneralProtectionFault));
    Idt::Set(Vector::PAGE_FAULT,
             Idt::InterruptDescriptor::ReturningWithError(HandlePageFault, Gdt::IST_FAULT_STACK));

    // The three faults that cannot trust the interrupted stack get their own via
    // the Interrupt Stack Table.
    Idt::Set(Vector::DOUBLE_FAULT, Idt::InterruptDescriptor::ReturningWithError(
                                       HandleDoubleFault, Gdt::IST_FAULT_STACK));
    Idt::Set(Vector::NON_MASKABLE_INTERRUPT,
             Idt::InterruptDescriptor::Returning(HandleNonMaskableInterrupt, Gdt::IST_FAULT_STACK));
    Idt::Set(Vector::MACHINE_CHECK,
             Idt::InterruptDescriptor::Returning(HandleMachineCheck, Gdt::IST_FAULT_STACK));

    Idt::Set(Vector::TIMER, Idt::InterruptDescriptor::Returning(HandleTimer));
    Idt::Set(Vector::KEYBOARD, Idt::InterruptDescriptor::Returning(HandleKeyboard));
    Idt::Set(Vector::ASSASSIN, Idt::InterruptDescriptor::Returning(HandleAssassin));
    Idt::Set(Vector::WAKEUP, Idt::InterruptDescriptor::Returning(HandleWakeup));
    Idt::Set(Vector::SPURIOUS, Idt::InterruptDescriptor::Returning(HandleSpurious));

    Idt::Load();
}

}  // namespace Interrupt
