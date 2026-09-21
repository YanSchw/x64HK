#include "Arch/Context.h"
#include "Debug/Panic.h"

/// Landing pad in case a kickoff function ever returns. It has no valid return
/// address of its own, so returning would jump into whatever the stack holds.
static void KickoffReturned() {
    PANIC("A thread returned from its kickoff function");
}

void PrepareContext(void* InStackTop, Context& OutContext, KickoffFunction InKickoff, uintptr_t InParam1,
                    uintptr_t InParam2, uintptr_t InParam3) {
    uintptr_t* stack = static_cast<uintptr_t*>(InStackTop);

    // The stack is built so that two chained `ret` instructions walk into the
    // thread body: ContextSwitch returns into FakeSystemVAbi, which returns into
    // the kickoff function.
    //
    // After those two pops, RSP is 8 modulo 16 at the kickoff entry, exactly as
    // if it had been reached by a call from 16-byte aligned code -- which the
    // SysV ABI requires and SSE spills would otherwise fault on.
    *(--stack) = reinterpret_cast<uintptr_t>(KickoffReturned);
    *(--stack) = reinterpret_cast<uintptr_t>(InKickoff);
    *(--stack) = reinterpret_cast<uintptr_t>(FakeSystemVAbi);

    OutContext = Context{
        .Rbx = 0,
        .Rbp = 0,
        .R12 = 0,
        .R13 = InParam1,
        .R14 = InParam2,
        .R15 = InParam3,
        .Rsp = stack,
    };
}
