# x64HK

An x86-64 hobby kernel. It boots from a legacy BIOS through GRUB, switches to
long mode, brings up every CPU core, and runs preemptive kernel threads on top
of a prologue/epilogue interrupt model.

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
| `make qemu` | Boot it in QEMU, with a GDB stub listening |
| `make iso` | Build `Build/x64HK.iso`, bootable and USB-flashable |
| `make qemu-iso` | Boot that ISO instead of the raw kernel |
| `make usb USB_DEVICE=…` | Write the ISO to a USB device |
| `make gdb` / `make connect-gdb` | Debug |
| `make test` | Run the boot time test suites in QEMU on 1 and 4 cores |
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

The ISO needs `grub-mkrescue` and `xorriso`:

```bash
brew install xorriso i686-elf-grub                  # macOS
apt install grub-pc-bin grub-common xorriso         # Debian
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

## Source layout

```
Kernel/
  Main.cpp       demo threads, screen layout, per-core debug streams
  Config.h       every tunable in one place
  Types.h        fixed-width types for a freestanding build

  Boot/          Multiboot headers and parsing, entry, long mode, SMP startup
  Arch/          CPU, GDT/IDT/TSS, ACPI, APIC, PIT, CMOS, CGA, serial, context
  Interrupt/     Guard, epilogues, IDT handlers
  Thread/        Thread, Dispatcher, Scheduler, IdleThread
  Sync/          SpinLock, TicketLock, Semaphore, Bellringer
  Device/        PS/2 keyboard and key decoding, text and serial streams
  Memory/        physical frame allocator, buddy heap
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

## Configuration

`Kernel/Config.h` holds everything worth changing:

| | Default | |
| --- | --- | --- |
| `MAX_CORES` | 8 | sizes every `PerCore<>` array |
| `SCHEDULER_TICK_MS` | 10 | LAPIC timer period |
| `THREAD_STACK_SIZE` | 16 KiB | interrupt frames land here too |
| `HEAP_LOG2` | 24 | 16 MiB heap, taken from the frame allocator |
| `IDENTITY_MAPPED_LIMIT` | 4 GiB | what the boot page tables reach, and so the frame allocator too |
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

- No UEFI, no higher-half mapping, no virtual memory past the boot identity map
- Physical memory above 4 GiB is reported but not used: the boot page tables
  only identity map that far, so the frame allocator stops there
- No user mode — everything runs in ring 0 and the GDT has no ring 3 segments
- No FPU or SSE, so context switches never have to save vector state
- One I/O APIC and flat logical APIC addressing, which caps `MAX_CORES` at 8
- No filesystem, no storage driver, no network
