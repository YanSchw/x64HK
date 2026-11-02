# x64HK user ABI

How a ring 3 program talks to the kernel. This document is normative: the kernel
and `ABI/C` both implement what is written here, and a hand-written assembly
blob that follows it needs nothing else.

Version **1**. See [Compatibility](#compatibility) for what may change.

## Contents

- [Execution environment](#execution-environment)
- [Calling convention](#calling-convention)
- [Return values and errors](#return-values-and-errors)
- [System calls](#system-calls)
- [Key encoding](#key-encoding)
- [Faults and termination](#faults-and-termination)
- [What ring 3 cannot do](#what-ring-3-cannot-do)
- [Kernel side](#kernel-side)
- [Compatibility](#compatibility)
- [The C library](#the-c-library)

## Execution environment

### Address space

A user program lives in the lower canonical half. The kernel's higher half is
mapped in the same tables but every entry on the path to it has U/S clear, so a
ring 3 access faults instead of reading kernel memory.

| | Address | |
| --- | --- | --- |
| `HK_USER_MIN` | `0x0000000000001000` | first address that may be mapped |
| `HK_USER_IMAGE_BASE` | `0x0000000000400000` | where the program image is loaded |
| `HK_USER_STACK_TOP` | `0x00007FFFFFFFF000` | top of the first task's stack |
| `HK_USER_STACK_SIZE` | 64 KiB | one guard page below it |
| `HK_USER_MAX` | `0x0000800000000000` | first non-user address |

Page zero is never mapped, so a null dereference is a page fault rather than a
silent read. The user stack, like a kernel stack, sits above an unmapped guard
page, and each task gets the next slot down — so `rsp` at entry is the top of
*its* stack, which is `HK_USER_STACK_TOP` only for the first one.

All tasks share one address space. The kernel's half is in the same tables
without U/S, which is what keeps ring 3 out of the kernel; keeping two ring 3
tasks out of each other needs a set of tables per task, and that is a later
milestone. Nothing in this document depends on which of the two it is.

The image is mapped W^X: text is read-execute, data is read-write and
no-execute. The stack is read-write and no-execute.

### Register state at entry

The kernel enters a fresh program at the image's entry point with:

| Register | Value |
| --- | --- |
| `rip` | entry point |
| `rsp` | `HK_USER_STACK_TOP`, 16 byte aligned |
| `rflags` | `0x202` — reserved bit 1, IF set |
| `cs` | `0x2b` (ring 3 code) |
| `ss` | `0x23` (ring 3 data) |
| everything else | zero |

Zeroing is not a courtesy: a leftover kernel pointer in a general purpose
register is an information leak, so every register the kernel does not
deliberately set is cleared before the first `sysretq`.

The stack is empty. There are no arguments, no environment and no auxiliary
vector — a program that wants input calls `GetKey`.

### The red zone

User code may use the 128 byte red zone below `rsp`. Nothing the kernel does
runs on the user stack: `syscall` switches to a kernel stack before it touches
memory, and an interrupt taken in ring 3 switches to `TSS.Rsp[0]`.

This is the opposite of the kernel, which is built `-mno-red-zone` precisely
because its interrupt frames do land on the current stack.

### Floating point

The kernel saves no vector state across a context switch, so **user code must
not use x87, MMX or SSE**. Compile with `-mgeneral-regs-only`. A program that
ignores this will see its registers change under it at an arbitrary
instruction boundary.

That also rules out passing or returning `float` and `double`, and rules out
the compiler's habit of inlining `memcpy` as SSE moves.

## Calling convention

`syscall` / `sysret`. Not `int 0x80`: there is no software interrupt gate for
user mode, and a `int` from ring 3 to any vector in the IDT raises a general
protection fault because every gate has DPL 0.

| | Register |
| --- | --- |
| Call number | `rax` |
| Argument 1 | `rdi` |
| Argument 2 | `rsi` |
| Argument 3 | `rdx` |
| Return value | `rax` |

This is the SysV argument order, so a C wrapper only has to move the call
number into `rax`. Arguments 4 and up would be `r10`, `r8`, `r9` — `r10` rather
than `rcx`, because `syscall` overwrites `rcx` — but no call in version 1 takes
more than three.

**Clobbered by every call:** `rax`, `rcx`, `r11`. The `syscall` instruction
itself parks the return address in `rcx` and `rflags` in `r11`; `rax` carries
the result. Every other register, and all of memory, comes back unchanged.

**Flags:** `IF`, `TF`, `DF`, `NT` and `AC` are cleared on entry by `SFMASK` and
restored from `r11` on return. Callers must not depend on any flag surviving a
call.

Unused argument registers must be zero. The kernel rejects a non-zero reserved
argument with `HK_EINVAL`, which is what makes it safe to give those registers
a meaning in a later version.

## Return values and errors

Every call returns a signed 64-bit value in `rax`. Negative means failure and
the magnitude is the error code; zero and positive are success, and what the
value means is per call.

```c
if (result < 0) { /* error, -result is an hk_error */ }
```

There is no `errno`. A single register carrying both outcomes is enough because
no call can legitimately return a value with the top bit set: lengths are
bounded by `HK_WRITE_MAX` and a key fits in 24 bits.

| Code | Name | Meaning |
| --- | --- | --- |
| 1 | `HK_EPERM` | the operation is not permitted from ring 3 |
| 2 | `HK_ENOSYS` | no such call number |
| 3 | `HK_EINVAL` | an argument value is not valid |
| 4 | `HK_EFAULT` | a pointer argument is outside the user address space |
| 5 | `HK_EBADF` | no such handle |
| 6 | `HK_EAGAIN` | the call would have blocked |
| 7 | `HK_ERANGE` | an argument is out of range |

`HK_EFAULT` is returned for a buffer that leaves the user half, that wraps
around the address space, or whose pages are not mapped for the access being
made. The kernel validates before it copies; a partly written buffer is not a
possible outcome of a failed `Write`.

## System calls

| `rax` | Name | Signature |
| --- | --- | --- |
| 0 | `Exit` | `noreturn void Exit(int32_t status)` |
| 1 | `Write` | `int64_t Write(uint64_t handle, const void* buffer, uint64_t length)` |
| 2 | `Yield` | `int64_t Yield(void)` |
| 3 | `GetKey` | `int64_t GetKey(void)` |

### 0 — Exit

Terminates the calling task. Does not return, so there is no error path; a
status outside `int32_t` is truncated rather than rejected.

The status is reported on the kernel's debug output. `0` is success by
convention (`HK_EXIT_SUCCESS`); statuses from `HK_EXIT_FAULT_BASE` (128) upwards
are reserved for the kernel to report a fault, see
[Faults and termination](#faults-and-termination).

### 1 — Write

Writes up to `HK_WRITE_MAX` (1024) bytes to a handle. Returns the number of
bytes written, which may be less than `length`; a caller that needs all of it
loops. Writing zero bytes is valid and returns zero.

| Handle | Name | Destination |
| --- | --- | --- |
| 1 | `HK_STDOUT` | the shared text window in the `Vault` |

Any other handle is `HK_EBADF`. Handle 0 is deliberately not valid, so a
zeroed-out argument cannot be mistaken for standard output.

Bytes are written as CGA characters at the cursor of the window the kernel
keeps for user output. `\n` starts a new line and the window scrolls when the
cursor runs past the bottom; every other byte is printed as-is. There is no
backspace handling — a program that wants it moves the cursor itself.

A null byte ends the write. The return value counts the bytes that were
actually consumed, so a caller passing a null-terminated string gets its length
back and a caller passing binary data finds out where it stopped.

The write is atomic with respect to other writers: the whole buffer reaches the
window under one `Guard`, so two tasks printing at once interleave between
calls but never inside one.

`HK_EFAULT` if the buffer is not readable from ring 3. The kernel checks every
page of it against the page tables before copying, so passing a kernel address
is refused rather than served. `HK_ERANGE` never happens — a long buffer
produces a short write instead.

### 2 — Yield

Puts the calling task at the back of its core's ready queue and schedules
someone else. Returns `0` once it runs again. Never fails.

A task that yields in a tight loop still makes progress: this is a scheduling
hint, not a block.

### 3 — GetKey

Blocks until a keystroke is available and returns it encoded as described
below. Always returns a valid key — `HK_KEY_INVALID` is not a possible result,
because the keyboard epilogue only enqueues keys that decoded.

This is the same queue the kernel's own `KeyboardThread` consumes from. Exactly
one waiter is woken per keystroke, so two tasks both calling `GetKey` split the
stream rather than each seeing every key.

There is no non-blocking form in version 1. The argument registers are reserved
and must be zero, which leaves room for a flags word later.

## Key encoding

A key is a 24-bit value packed into the return register:

```
 23        16 15         8 7          0
+------------+------------+------------+
| modifiers  | scan code  |   ascii    |
+------------+------------+------------+
```

| Field | Bits | |
| --- | --- | --- |
| `ascii` | 7..0 | US layout character, or `0` for a key that produces none |
| `code` | 15..8 | scan code, an `hk_scancode` |
| `modifiers` | 23..16 | modifier state when the key was pressed |

| Bit | Modifier |
| --- | --- |
| 0 | `HK_MOD_SHIFT` |
| 1 | `HK_MOD_ALT_LEFT` |
| 2 | `HK_MOD_ALT_RIGHT` |
| 3 | `HK_MOD_CTRL_LEFT` |
| 4 | `HK_MOD_CTRL_RIGHT` |
| 5 | `HK_MOD_CAPS_LOCK` |
| 6 | `HK_MOD_NUM_LOCK` |
| 7 | `HK_MOD_SCROLL_LOCK` |

The scan codes are the set 1 make codes renumbered densely, and they are the
same values as `Key::ScanCode` in `Kernel/Device/Key.h`. That file is the
source they were generated from; the two must not drift, and the kernel pins
the count with a static assertion.

Cursor keys and the numeric keypad share make codes and are told apart by the
`0xe0` prefix during decoding, which is why `HK_KEY_UP` and `HK_KEY_KP_8` are
the same number. The `ascii` field already reflects the distinction.

## Faults and termination

A fault in ring 3 kills the task. The same fault in ring 0 panics the machine.
The two are told apart by the privilege level in the pushed `cs`:

```c
const bool fromUser = (InContext->Cs & 3) == 3;
```

The pushed selector is authoritative — it is the CPL at the moment of the
fault, which is exactly the question being asked. The faulting address, the
instruction pointer and the error code are not: a user program controls all
three.

When a task is killed the kernel prints a one line diagnostic naming the fault,
the task and the faulting address, and then takes it out of circulation. The
other cores are not disturbed, and the kernel keeps running.

Taking it down cannot happen in the handler itself: a handler is a prologue,
and ending a thread needs the Guard. So the handler rewrites the interrupt
frame — `cs`, `ss`, `rflags`, `rsp` and `rip` — to return into the kernel
instead of into the faulting instruction, and the work happens at thread level
on the stack the thread already owns. In 64-bit mode `iretq` pops `ss:rsp` even
when the privilege level does not change, so the stack switch comes for free.

The reported exit status is `HK_EXIT_FAULT_BASE + vector`:

| Status | Fault |
| --- | --- |
| 128 | divide error |
| 134 | invalid opcode |
| 140 | stack segment fault |
| 141 | general protection fault |
| 142 | page fault |

which is 128 plus the architectural vector number, the same shape as a shell's
128 plus signal.

A task cannot survive its own fault. There is no signal mechanism and no way to
install a handler.

## What ring 3 cannot do

Each of these raises a fault that kills the task, and each is covered by a test
that asserts exactly that.

| Attempt | Result |
| --- | --- |
| `cli`, `sti` | general protection fault — `IOPL` is 0 |
| `hlt`, `lgdt`, `lidt`, `ltr`, `wrmsr`, `mov cr*` | general protection fault — privileged |
| `in`, `out` on any port | general protection fault — the TSS I/O bitmap base is past the limit, so no port is permitted |
| reading or writing kernel memory | page fault with `user: 1` — U/S is clear on the kernel's entries |
| executing the stack or a data page | page fault with `ifetch: 1` — NX |
| writing its own text | page fault with `write: 1` |
| `int` on any vector | general protection fault — every IDT gate has DPL 0 |
| `syscall` with an unknown number | `HK_ENOSYS`, not a fault |

The I/O bitmap deserves the note: `TaskStateSegment::IoMapBase` is set to
`sizeof(TaskStateSegment)`, which is past the segment limit. The CPU reads an
offset beyond the limit as "no permission", so ring 3 has no port access at all
without a single bit of bitmap being stored.

## Kernel side

What M4 has to put in place for the above to hold. Listed here because the
values are part of the ABI — a blob that hard-codes `0x2b` in a far return
depends on them.

### GDT

`syscall` and `sysret` do not read descriptors. They compute selectors from
`STAR` by fixed offsets, which dictates the order of the table:

| Offset | Descriptor |
| --- | --- |
| `0x00` | null |
| `0x08` | kernel code, 64 bit, DPL 0 |
| `0x10` | kernel data, DPL 0 |
| `0x18` | user code, 32 bit, DPL 3 |
| `0x20` | user data, DPL 3 |
| `0x28` | user code, 64 bit, DPL 3 |
| `0x30` | first TSS, two slots per core |

The 32-bit user code descriptor is never used — there is no compatibility mode
here — but `sysretl` would load it, so the slot has to exist for the offsets to
come out right. `Gdt::FIRST_TSS` moves from `0x18` to `0x30`.

### MSRs, per core

| MSR | Value | |
| --- | --- | --- |
| `EFER` | `\|= SCE` | without it `syscall` is an invalid opcode |
| `STAR` | `0x001B_0008_0000_0000` | `[47:32]` = `0x0008`, `[63:48]` = `0x001b` |
| `LSTAR` | `&SyscallEntry` | the ring 0 entry point |
| `SFMASK` | `0x44700` | `TF\|IF\|DF\|NT\|AC` |
| `KERNEL_GS_BASE` | `&s_SyscallArea[core]` | swapped in by `swapgs` |

`syscall` derives `cs = STAR[47:32]` and `ss = STAR[47:32] + 8`, which the
existing kernel selectors already satisfy. `sysretq` derives
`cs = STAR[63:48] + 16` and `ss = STAR[63:48] + 8`, both forced to RPL 3, which
is where `0x2b` and `0x23` come from.

### Entry and exit

`syscall` changes no stack. It clears the masked flags, loads `cs`/`ss` from
`STAR`, and jumps to `LSTAR` — with `rsp` still pointing at the user stack,
which the kernel must not touch. The entry sequence is therefore fixed:

```asm
swapgs
mov [gs:8], rsp          ; park the user stack
mov rsp, [gs:0]          ; this core's kernel stack
push qword [gs:8]        ; and straight onto it
swapgs
```

```c
struct SyscallArea {
    uint64_t KernelStack;   // +0, same value as TSS.Rsp[0]
    uint64_t UserStack;     // +8, scratch for the sequence above
};
```

The two `swapgs` instructions bracket four, and that is deliberate. The scratch
slot is per core, not per task, so a system call that blocks would lose its
parked stack pointer to whichever task calls next — hence the `push` that moves
it onto the kernel stack immediately. And putting `gs` back right away keeps
one invariant true everywhere else: **outside these four instructions, `GS.base`
is always zero and `KERNEL_GS_BASE` always points at the `Area`.**

That invariant is what makes an interrupt taken in ring 3 need no `swapgs` of
its own. The alternative — swapping on entry and back on exit, the way Linux
does — would leave the MSRs in a state that depends on what a core was doing
when it was preempted, and a handler written with `[[gnu::interrupt]]` has
nowhere to put the matching `swapgs`.

Nothing can interrupt the bracket, because `SFMASK` cleared `IF` and both slots
are kernel pages that are always mapped. Interrupts go back on after the switch
is complete, which is what lets `GetKey` block.

Returning is `sysretq`, with `rcx` holding the user `rip` and `r11` the user
`rflags`. No `swapgs` is needed there: the invariant already holds.

Two things about `sysretq` are worth being explicit about, because both are
classic ways to fault in ring 0:

- **`rcx` must be canonical**, or `sysretq` faults with the privilege level
  already back at 3 and the stack still the kernel's. Nothing here can make it
  non-canonical: `rcx` is only ever the value `syscall` itself put there, saved
  on entry and popped back on exit, and that is by definition an address in the
  ring 3 program. A path that ever returned a user-supplied `rcx` would have to
  use `iretq` instead.
- **`r11` becomes `rflags`.** `sysretq` loads `(r11 & 0x3C7FD7) | 2`, which
  clears `VM` and `RF` and forces bit 1 on, but does pass `IF` and `IOPL`
  through. That is still safe, and not because of the mask: `syscall`
  *overwrites* `r11` with the caller's own `rflags` on the way in, so the value
  restored on the way out is the one the CPU captured, never one the program
  wrote. And a ring 3 program cannot raise its own `IOPL` in the first place —
  `popfq` below `IOPL` silently ignores that field.

An interrupt taken in ring 3 is unrelated to all of this: it switches to
`TSS.Rsp[0]` by hardware and returns with `iretq`, exactly as it already does
for ring 0. `TSS.Rsp[0]` has to be reloaded on every context switch, because it
names the stack of the task that is running now.

### GS

`GS` belongs to the entry sequence above and to nothing else, which is what
makes the short bracket possible. If a handler ever needs per core state it
gets it from `Cpu::GetId()`, which reads the LAPIC instead.

A ring 3 program cannot disturb any of this. `wrmsr` is privileged, `FSGSBASE`
is not enabled in CR4, and `EnterUserMode` loads `ds` and `es` from a
descriptor with base zero.

## Compatibility

Version 1 is not frozen; the kernel it belongs to has no users to break. What
the design does guarantee is that the following can be added without renumbering
anything:

- new call numbers, appended
- new arguments in the reserved registers, which are checked to be zero today
- new handles for `Write`, which are `HK_EBADF` today
- new error codes, appended
- new scan codes before `HK_KEY_COUNT`

Changing the meaning of an existing number is a version bump. `HK_ABI_VERSION`
in `ABI/C/HK.h` carries it.

## The C library

`ABI/C/HK.h` and `ABI/C/HK.c` implement this document for user programs. The
header is valid C11 and C++11 and needs no other header; the implementation is
freestanding C and links against nothing.

```bash
clang --target=x86_64-pc-linux-gnu -ffreestanding -nostdlib -nostartfiles \
      -fno-pic -no-pie -mno-sse -mno-mmx -mgeneral-regs-only -mcmodel=small \
      -IABI/C -fuse-ld=lld -Wl,-e,_start -Wl,--image-base=0x400000 \
      -o Hello.elf Hello.c ABI/C/HK.c
```

Three of those are not optional:

- `-mcmodel=small`, not the kernel's `-mcmodel=kernel`. A user program lives in
  the low half.
- `-no-pie`. `lld` defaults to a position independent executable and then
  rejects the absolute relocations a non-PIC build produces.
- `--image-base=0x400000` rather than `-Ttext`. `-Ttext` moves the code but
  leaves the ELF headers and read-only data at the linker's default base, which
  scatters the image over two widely separated addresses for no reason.

A C++ program is built the same way with `clang++ -std=c++23 -fno-rtti
-fno-exceptions`, but `HK.c` is compiled separately as C. `main` gets C linkage
from the header, so `_start` finds it either way.

The library does not supply `memcpy`, `memset`, `memmove` or `memcmp`. The
compiler emits calls to them for large struct copies and array initialisers
even under `-ffreestanding`, and a program that trips over that has to provide
them, the same way `Kernel/Compiler/Libc.cpp` does for the kernel.

The C API is prefixed `hk_`. Compiled as C++ the same header additionally
declares a `namespace HK` of inline wrappers over it — `HK::Write`,
`HK::GetKey`, a `HK::Key` with accessors — so C++ user programs read like C++
without a second implementation existing.

`HK.c` also supplies `_start`, which calls `main` and passes its return value
to `hk_exit`. Define `HK_NO_START` to leave the entry point to a blob of your
own.
