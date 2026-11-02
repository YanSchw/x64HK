# x64HK

An x86-64 hobby kernel. It boots from a legacy BIOS through GRUB, switches to
long mode, brings up every CPU core, runs preemptive kernel threads on top of a
prologue/epilogue interrupt model, and drops into ring 3 to run a user program
against a documented system call ABI.

Written in freestanding C++23 and NASM, with no libc and no external
dependencies.

## Quick start

```bash
brew install llvm nasm qemu          # macOS
# apt install clang lld llvm nasm qemu-system-x86  # Debian

make qemu
```

## Building

| Target | What it does |
| --- | --- |
| `make` | Build `Build/Kernel.elf` |
| `make user` | Build just the ring 3 demo program |
| `make qemu` | Boot it in QEMU, with a GDB stub listening |
| `make iso` | Build `Build/x64HK.iso`, bootable and USB-flashable |
| `make qemu-iso` | Boot that ISO instead of the raw kernel |
| `make usb USB_DEVICE=…` | Write the ISO to a USB device |
| `make gdb` / `make connect-gdb` | Debug |
| `make test` | Run the boot time test suites in QEMU on 1 and 4 cores |
| `make test-iso` | Run them again over the GRUB handover |
| `make test-overflow` | Check that a stack overflow is caught |
| `make test-wprotect` | Check that `.text` really is read only |
| `make clean` | Remove `Build/` |
| `make help` | The full list |

Every target has five flavours, each building into its own directory so they
never collide:

| Suffix | Flags |
| --- | --- |
| `-opt` | `-O3 -flto -march=x86-64-v2 -DNDEBUG` |
| `-dbg` | `-Og -fno-omit-frame-pointer` |
| `-noopt` | `-O0` |
| `-verbose` | `-DVERBOSE`, enables the `DBG_VERBOSE` output |
| `-test` | `-O1 -DTEST`, compiles `Kernel/Test` in |

So `make qemu-dbg`, `make iso-opt`, `make qemu-serial-verbose`.

`make VERBOSE=` shows the actual compiler command lines.

## Booting

The image carries two Multiboot headers, and the kernel notices which one it was
booted through.

**Multiboot2** is the real target. It is the only handover that hands over the
ACPI RSDP directly — otherwise the kernel has to go looking for it in the BIOS
area — and it supplies a proper memory map. It is what GRUB uses:

```bash
make iso
make qemu-iso
```

The ISO needs `grub-mkrescue` and `xorriso`, plus `mtools` wherever GRUB's EFI
platform is installed (grub-mkrescue then builds an EFI image as well):

```bash
brew install xorriso i686-elf-grub                  # macOS
apt install grub-pc-bin grub-common xorriso mtools  # Debian
```

**Multiboot1** exists purely so QEMU's built-in `-kernel` loader can boot the
raw ELF. That turns the edit-test cycle into about a second instead of a full
ISO rebuild, which is worth the twelve bytes of header.

There is deliberately no framebuffer tag in the Multiboot2 header, and the
console flags tag declares EGA text support, so the boot loader leaves the
machine in text mode and `0xb8000` stays the screen.

### USB

`grub-mkrescue` produces an isohybrid image — the ISO carries a real MBR, so it
boots from a stick byte for byte:

```bash
make usb USB_DEVICE=/dev/disk4     # macOS
make usb USB_DEVICE=/dev/sdb       # Linux
```

Without `USB_DEVICE` the target lists the attached devices and stops. With one,
it warns, waits five seconds, and then needs `sudo` for the `dd`.

Boot the stick in **legacy/CSM mode**. There is no UEFI support.

### Debugging

`make qemu` always exposes a GDB stub — on a unix socket under
`$XDG_RUNTIME_DIR` if that exists, otherwise on TCP port `uid + 1024`.

```bash
make qemu-gdb       # start halted, wait for a debugger
make connect-gdb    # in another terminal
```

`make gdb` does both in one go. The symbols come from `Build/Kernel.elf64`; the
32-bit `Kernel.elf` that boot loaders consume is the same image with the ELF
header rewritten.

## User mode

The kernel ends up in ring 3 with a program of its own. `User/Hello.c` is a
freestanding C program that knows nothing about the kernel beyond
[`ABI/SPECIFICATION.md`](ABI/SPECIFICATION.md): it links against `ABI/C`, is
embedded in the kernel image, and every character it puts on the screen goes
through a `syscall`.

```
ring 3   User/Hello.c  ->  ABI/C/HK.c  ->  syscall
ring 0   Arch/Syscall.asm  ->  SyscallDispatch  ->  Exit / Write / Yield / GetKey
```

Four calls, `rax` for the number and `rdi`/`rsi`/`rdx` for the arguments, a
negative return meaning failure. The keyboard feeds `GetKey` rather than a
kernel thread, so typing in QEMU is served by a user process.

The header is shared by both sides, which is why the kernel carries
`static_assert`s that its own `Key::ScanCode` still matches `hk_scancode`.

A fault in ring 3 kills the task and leaves the kernel running; the same fault
in ring 0 still panics. The two are told apart by the privilege level in the
pushed `cs`.

## Source layout

```
ABI/             the ring 3 contract: SPECIFICATION.md and the C/C++ binding
User/            the demo program that runs against it

Kernel/
  Main.cpp       demo threads, screen layout, per-core debug streams
  Config.h       every tunable in one place
  Types.h        fixed-width types for a freestanding build

  Boot/          Multiboot headers and parsing, entry, long mode, SMP startup
  Arch/          CPU, GDT/IDT/TSS, ACPI, APIC, PIT, CMOS, CGA, serial, context
  Interrupt/     Guard, epilogues, IDT handlers
  Thread/        Thread, Dispatcher, Scheduler
  Sync/          SpinLock, TicketLock, Semaphore, Bellringer
  Device/        PS/2 keyboard and key decoding, text and serial streams
  Memory/        physical frame allocator, page tables, buddy heap
  User/          ELF loading, ring 3 tasks, the embedded demo program
  Lib/           Queue, RingBuffer, PerCore, OutputStream, string functions
  Debug/         assertions, panic, per-core debug output
  Compiler/      global constructors, operator new/delete
  App/           the demo threads
  Test/          boot time test suites, TEST builds only

Tools/           Makefile fragments: build, image, QEMU
```

## Testing

```bash
make test
```

Builds a `-test` flavour with `Kernel/Test` compiled in, boots it headless, and
runs the suites: the pure ones on the bootstrap core before threading, the rest
on a thread once the scheduler is up. The kernel reports through QEMU's
`isa-debug-exit` device, so a failed check, a failed `ASSERT` or a panic all
come back as a non-zero exit status.

The user mode suite is the odd one out. Every rule ring 3 has to obey gets
checked by breaking it: `Kernel/Test/UserProbes.asm` holds fifteen short
programs — `cli`, `hlt`, `out`, `int`, a read of kernel memory, a write to
their own code, a jump into their own stack — which the suite maps into user
pages, runs as tasks, and then asks how each one died. Anything that must fault
is followed by `ud2`, so an instruction that wrongly succeeds reports a
different fault instead of passing quietly.

## Configuration

`Kernel/Config.h` holds everything worth changing:

| | Default | |
| --- | --- | --- |
| `MAX_CORES` | 8 | sizes every `PerCore<>` array |
| `SCHEDULER_TICK_MS` | 10 | LAPIC timer period |
| `THREAD_STACK_SIZE` | 16 KiB | interrupt frames land here too |
| `HEAP_LOG2` | 24 | 16 MiB heap, taken from the frame allocator |
| `IDENTITY_MAPPED_LIMIT` | 4 GiB | what the boot page tables reach, before the kernel builds its own |
| `KERNEL_VMA` | `0xFFFFFFFF80000000` | where the kernel is linked |
| `DIRECT_MAP_BASE` | `0xFFFF800000000000` | where all of RAM is mapped |
| `AP_TRAMPOLINE_ADDRESS` | `0x40000` | must be page aligned, below 1 MiB |

## Conventions

| Element | Convention |
| --- | --- |
| Files | `Foo.h`, `Foo.cpp`, `Foo.asm`, named after the primary type |
| Classes | `PascalCase` |
| Members | `m_MemberVariable` |
| Globals/statics | `s_GlobalVariable` |
| Locals | `localVariable` |
| Constants | `UPPER_SNAKE_CASE` |
| Macros | `MACRO()` |
| Functions | `FunctionName(InReadOnlyParam, OutResultParam)` |

Comments explain x86, not C++.

Four spaces for indentation, `Tools/Untabify.sh --check` fails if a tab creeps back into a source file.

## Not there yet

- No UEFI
- Page tables are 4 KiB throughout. No large pages, which costs about 0.2% of
  RAM in tables
- One address space. Ring 3 cannot reach the kernel, but two user tasks can
  reach each other — that needs a set of page tables per task
- No `fork`, no `exec`, no filesystem: the one user program is linked into the
  kernel image
- No FPU or SSE, so context switches never have to save vector state
- One I/O APIC and flat logical APIC addressing, which caps `MAX_CORES` at 8
- No filesystem, no storage driver, no network
