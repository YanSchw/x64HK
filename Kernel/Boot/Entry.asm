; Kernel entry point.
;
; The boot loader has already put us in 32-bit protected mode with paging off
; and a flat, if borrowed, GDT. All this code does is take ownership of the
; machine state that C++ cannot set up for itself -- GDT, stack, direction flag
; -- and then hand over to the long mode switch.

[BITS 32]

[EXTERN CoreStackSizePhys]     ; Arch/Cpu.cpp, through Boot/Sections.ld
[EXTERN CoreStackPointerPhys]  ; Arch/Cpu.cpp, through Boot/Sections.ld
[EXTERN LongMode]              ; Boot/LongMode.asm
[EXTERN MultibootMagicPhys]    ; Boot/Multiboot.cpp, through Boot/Sections.ld
[EXTERN MultibootInfoPhys]     ; Boot/Multiboot.cpp, through Boot/Sections.ld

%include "Boot/Multiboot.inc"

BOOT_CODE64 equ 0x08
BOOT_DATA64 equ 0x10
BOOT_CODE32 equ 0x18
BOOT_DATA32 equ 0x20

; Low 32 bits of Config::KERNEL_VMA. Subtracting it from the low half of a
; kernel address gives the physical one, which is all 32-bit code can use.
KERNEL_VMA_LOW equ 0x80000000

[SECTION .boot]

[GLOBAL StartupBsp]
StartupBsp:
    ; eax holds the loader magic, ebx a pointer to the boot information. Both
    ; are only valid right now, so stash them before touching anything else.
    mov [MultibootMagicPhys], eax
    mov [MultibootInfoPhys], ebx

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
    lgdt [BootGdtPointer]

    mov ax, BOOT_DATA32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; A far jump is the only way to reload CS.
    jmp BOOT_CODE32:.LoadCodeSegment

.LoadCodeSegment:
    ; Claim a boot stack. The atomic exchange-and-add is what keeps two cores
    ; from picking the same slice when the APs come up in parallel.
    mov eax, [CoreStackSizePhys]
    lock xadd [CoreStackPointerPhys], eax
    ; Stacks grow down, so move to the far end of the claimed slice.
    add eax, [CoreStackSizePhys]
    sub eax, KERNEL_VMA_LOW
    mov esp, eax

    ; The ABI requires DF clear on entry to any C function.
    cld

    jmp LongMode

; The real table lives in C++ at a higher half address that no 32-bit lgdt can
; reach, so the boot path carries its own. The two long mode selectors match
; Gdt::Selector, which is what lets Gdt::Load swap the tables without also
; having to reload cs.
align 8
[GLOBAL BootGdt]
BootGdt:
    dq 0
    dq 0x00af9a000000ffff  ; 0x08  64-bit code
    dq 0x00af92000000ffff  ; 0x10  64-bit data
    dq 0x00cf9a000000ffff  ; 0x18  32-bit code
    dq 0x00cf92000000ffff  ; 0x20  32-bit data
.End:

[GLOBAL BootGdtPointer]
BootGdtPointer:
    dw BootGdt.End - BootGdt - 1
    dd BootGdt
