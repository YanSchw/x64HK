; Kernel entry point.
;
; The boot loader has already put us in 32-bit protected mode with paging off
; and a flat, if borrowed, GDT. All this code does is take ownership of the
; machine state that C++ cannot set up for itself -- GDT, stack, direction flag
; -- and then hand over to the long mode switch.

[BITS 32]

[EXTERN CoreStackSize]            ; Arch/Cpu.cpp
[EXTERN CoreStackPointer]         ; Arch/Cpu.cpp
[EXTERN GdtProtectedModePointer]  ; Arch/Gdt.cpp
[EXTERN LongMode]                 ; Boot/LongMode.asm
[EXTERN MultibootMagic]           ; Boot/Multiboot.cpp
[EXTERN MultibootInfo]            ; Boot/Multiboot.cpp

%include "Boot/Multiboot.inc"

[SECTION .text]

[GLOBAL StartupBsp]
StartupBsp:
    ; eax holds the loader magic, ebx a pointer to the boot information. Both
    ; are only valid right now, so stash them before touching anything else.
    mov [MultibootMagic], eax
    mov [MultibootInfo], ebx

    cli
    ; Mask the non-maskable interrupt. Bit 7 of the CMOS index port does this,
    ; and there is no IDT yet to catch one.
    mov al, 0x80
    out 0x70, al

    jmp SegmentInit

; Shared by the bootstrap processor and, after its real mode preamble, by every
; application processor.
[GLOBAL SegmentInit]
SegmentInit:
    lgdt [GdtProtectedModePointer]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; A far jump is the only way to reload CS.
    jmp 0x08:.LoadCodeSegment

.LoadCodeSegment:
    ; Claim a boot stack. The atomic exchange-and-add is what keeps two cores
    ; from picking the same slice when the APs come up in parallel.
    mov eax, [CoreStackSize]
    lock xadd [CoreStackPointer], eax
    ; Stacks grow down, so move to the far end of the claimed slice.
    add eax, [CoreStackSize]
    mov esp, eax

    ; The ABI requires DF clear on entry to any C function.
    cld

    jmp LongMode
