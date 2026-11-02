#pragma once
#include "Types.h"
#include "Config.h"
#include "Arch/Context.h"

// A thread: a name, a stack, a saved register context and the address it starts
// running at.
//
// The entry is a plain function pointer plus an argument, so a thread body is an
// ordinary function and a thread carries no vtable. That matters beyond taste:
// the fault handler reads Name() off a thread whose stack has just run into its
// guard page, and a name that is a member rather than a virtual call cannot be
// wrong.
class Thread {
public:
    using Entry = void (*)(void* InArgument);

    Thread() = default;
    Thread(const char* InName, Entry InEntry, void* InArgument = nullptr)
        : m_Name(InName), m_Entry(InEntry), m_Argument(InArgument) {}

    // Copying a thread would duplicate a stack that contains absolute pointers
    // into itself, so it is not allowed.
    Thread(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread& operator=(Thread&&) = delete;

    const char* Name() const { return m_Name; }

    /// Maps a stack and lays out the first context. Idempotent, and separate
    /// from the constructor because static threads exist before the allocators
    /// they need.
    void Prepare();

    /// Same, for a thread that was default constructed. The per core idle
    /// threads live in an array and cannot carry constructor arguments.
    void Prepare(const char* InName, Entry InEntry, void* InArgument = nullptr);

    bool IsPrepared() const { return m_StackTop != 0; }

    /// Where interrupts and system calls taken in ring 3 land.
    uintptr_t StackTop() const { return m_StackTop; }

    /// Activates this thread on the calling core, abandoning the current stack.
    /// Used once per core to leave the boot stack.
    [[noreturn]] void Go();

    /// Saves this thread's registers and switches to InNext.
    void Resume(Thread* InNext);

    /// Marked by Scheduler::Kill; checked before the thread is scheduled again.
    bool IsDying() const { return __atomic_load_n(&m_Dying, __ATOMIC_ACQUIRE); }
    void MarkDying() { __atomic_store_n(&m_Dying, true, __ATOMIC_RELEASE); }

    /// Link field for Lib/Queue. Public because a member pointer template
    /// argument has to be accessible at the point of use. A thread is only ever
    /// in one queue at a time -- ready, or waiting on one semaphore.
    Thread* m_QueueLink = nullptr;

private:
    /// First thing a new thread executes. It exists because a thread starts by
    /// returning into code rather than being called, so there has to be
    /// something that turns the raw entry into a call.
    [[noreturn]] static void Kickoff(uintptr_t InThread, uintptr_t InParam2, uintptr_t InParam3);

    const char* m_Name = "thread";
    Entry m_Entry = nullptr;
    void* m_Argument = nullptr;

    Context m_Context{};
    bool m_Dying = false;
    uintptr_t m_StackTop = 0;
};
