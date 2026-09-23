; Application processor bring-up.
;
; Unlike the bootstrap processor, the other cores are never touched by the boot
; loader. A Startup-IPI drops them into real mode at a physical address given as
; a page number, which is why this code has to be relocated below 1 MiB before
; it runs (SmpBoot.cpp does that) and why it cannot use absolute addresses.

[SECTION .ap_trampoline]
[GLOBAL ApTrampolineGdt]
[GLOBAL ApTrampolineGdtPointer]

[BITS 16]

ApTrampolineStart:
    ; CS was set by the Startup-IPI; mirror it into DS so the [label - base]
    ; offsets below resolve against the relocated copy. No stack is used.
    mov ax, cs
    mov ds, ax

    cli
    ; Mask the NMI (bit 7 of the CMOS index port).
    mov al, 0x80
    out 0x70, al

    lgdt [ApTrampolineGdtPointer - ApTrampolineStart]

    ; Protection Enable in CR0, then a far jump to flush the prefetch queue and
    ; load a 32-bit code segment.
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:StartupAp

; Filled in by SmpBoot::RelocateTrampoline, because the descriptors have to
; describe the relocated address rather than the link-time one.
align 8
ApTrampolineGdt:
    dq 0, 0, 0, 0, 0
ApTrampolineGdtPointer:
    dw 0, 0, 0, 0, 0

[SECTION .boot]
[BITS 32]

[EXTERN SegmentInit]  ; Boot/Entry.asm

StartupAp:
    ; The selectors still point into the temporary real mode GDT, so reload them
    ; before handing over to the shared path.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp SegmentInit
