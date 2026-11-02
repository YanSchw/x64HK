#include "Arch/Syscall.h"
#include "ABI/C/HK.h"
#include "Arch/Gdt.h"
#include "Arch/Msr.h"
#include "Debug/Output.h"
#include "Device/Key.h"
#include "Interrupt/Guard.h"
#include "Lib/Math.h"
#include "Lib/PerCore.h"
#include "User/Task.h"

extern "C" void SyscallEntry();

namespace Syscall {

static_assert(__builtin_offsetof(Area, KernelStack) == 0, "Syscall.asm expects KernelStack at 0");
static_assert(__builtin_offsetof(Area, UserStack) == 8, "Syscall.asm expects UserStack at 8");

// The key encoding is shared with ring 3, so the two enums have to agree.
static_assert(ToUnderlying(Key::ScanCode::COUNT) == HK_KEY_COUNT, "Key.h and HK.h disagree");
static_assert(ToUnderlying(Key::ScanCode::ESCAPE) == HK_KEY_ESCAPE, "Key.h and HK.h disagree");
static_assert(ToUnderlying(Key::ScanCode::KP_EQUAL) == HK_KEY_KP_EQUAL, "Key.h and HK.h disagree");

/// sysret derives cs from STAR[63:48] + 16 and ss from + 8, syscall takes cs
/// from STAR[47:32] and ss from + 8. Arch/Gdt.h is laid out to match.
constexpr uint64_t STAR_VALUE = (uint64_t{Gdt::USER_CODE32 | Gdt::RING3} << 48) |
                                (uint64_t{Gdt::KERNEL_CODE} << 32);

/// Cleared on entry, restored from r11 on return. Interrupts off until the
/// stack switch is done; the rest are flags no kernel code wants inherited.
constexpr uint64_t MASKED_FLAGS = 0x100     // TF, single step
                                  | 0x200   // IF
                                  | 0x400   // DF, string direction
                                  | 0x4000  // NT, nested task
                                  | 0x40000;  // AC, alignment check

static PerCore<Area> s_Area;

void Initialize(unsigned InCoreId) {
    using Efer = Cpu::Msr<Cpu::MsrIndex::EFER>;
    Efer::Write(Efer::Read() | ToUnderlying(Cpu::MsrEfer::SCE));

    Cpu::Msr<Cpu::MsrIndex::STAR>::Write(STAR_VALUE);
    Cpu::Msr<Cpu::MsrIndex::LSTAR>::Write(reinterpret_cast<uintptr_t>(&SyscallEntry));
    Cpu::Msr<Cpu::MsrIndex::SFMASK>::Write(MASKED_FLAGS);
    Cpu::Msr<Cpu::MsrIndex::KERNEL_GS_BASE>::Write(reinterpret_cast<uintptr_t>(&s_Area[InCoreId]));
}

void SetKernelStack(unsigned InCoreId, uintptr_t InStackTop) {
    s_Area[InCoreId].KernelStack = InStackTop;
}

// --- The calls --------------------------------------------------------------

static int64_t Exit(uint64_t InStatus, uint64_t, uint64_t) {
    User::Terminate("exited", static_cast<int32_t>(InStatus));
}

static int64_t Write(uint64_t InHandle, uint64_t InBuffer, uint64_t InLength) {
    if (InHandle != HK_STDOUT) {
        return -HK_EBADF;
    }

    const size_t length = Math::Min<uint64_t>(InLength, HK_WRITE_MAX);
    char buffer[HK_WRITE_MAX];
    if (!User::CopyIn(buffer, InBuffer, length)) {
        return -HK_EFAULT;
    }

    // Print stops at a null byte, so that is where the write stops too.
    size_t written = 0;
    while (written < length && buffer[written] != '\0') {
        written++;
    }

    Guarded guard = Guard::Enter();
    guard.Vault().UserOutput.Print(buffer, Cga::Attribute(), written);

    return static_cast<int64_t>(written);
}

static int64_t Yield(uint64_t, uint64_t, uint64_t) {
    Guarded guard = Guard::Enter();
    guard.Vault().Scheduler.Resume();
    return 0;
}

static uint8_t ModifiersOf(const Key& InKey) {
    uint8_t bits = 0;
    bits |= InKey.Shift ? HK_MOD_SHIFT : 0;
    bits |= InKey.AltLeft ? HK_MOD_ALT_LEFT : 0;
    bits |= InKey.AltRight ? HK_MOD_ALT_RIGHT : 0;
    bits |= InKey.CtrlLeft ? HK_MOD_CTRL_LEFT : 0;
    bits |= InKey.CtrlRight ? HK_MOD_CTRL_RIGHT : 0;
    bits |= InKey.CapsLock ? HK_MOD_CAPS_LOCK : 0;
    bits |= InKey.NumLock ? HK_MOD_NUM_LOCK : 0;
    bits |= InKey.ScrollLock ? HK_MOD_SCROLL_LOCK : 0;
    return bits;
}

static int64_t GetKey(uint64_t, uint64_t, uint64_t) {
    Guarded guard = Guard::Enter();
    Vault& vault = guard.Vault();

    Key key;
    do {
        vault.KeysAvailable.P(vault);
    } while (!vault.Keys.Consume(key));

    return hk_key_pack(ToUnderlying(key.Code), key.Ascii(), ModifiersOf(key));
}

// --- Dispatch ---------------------------------------------------------------

using Handler = int64_t (*)(uint64_t, uint64_t, uint64_t);

struct TableEntry {
    Handler Call;
    unsigned Arity;  ///< Arguments the call reads; the rest have to be zero
};

static const TableEntry TABLE[] = {
    {Exit, 1},
    {Write, 3},
    {Yield, 0},
    {GetKey, 0},
};
static_assert(ArraySize(TABLE) == HK_SYS_COUNT, "The table and HK.h disagree");

/// Arguments a call does not read are reserved, and rejecting a non-zero one is
/// what makes it safe to give them a meaning later.
static bool ReservedAreZero(const TableEntry& InEntry, uint64_t InArg1, uint64_t InArg2,
                            uint64_t InArg3) {
    const uint64_t arguments[] = {InArg1, InArg2, InArg3};
    for (unsigned index = InEntry.Arity; index < ArraySize(arguments); index++) {
        if (arguments[index] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace Syscall

extern "C" int64_t SyscallDispatch(uint64_t InNumber, uint64_t InArg1, uint64_t InArg2,
                                   uint64_t InArg3) {
    if (InNumber >= ArraySize(Syscall::TABLE)) {
        return -HK_ENOSYS;
    }

    const Syscall::TableEntry& entry = Syscall::TABLE[InNumber];
    if (!Syscall::ReservedAreZero(entry, InArg1, InArg2, InArg3)) {
        return -HK_EINVAL;
    }

    return entry.Call(InArg1, InArg2, InArg3);
}
