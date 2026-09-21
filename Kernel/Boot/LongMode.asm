; Protected mode (32-bit) to long mode (64-bit).
;
; Long mode requires paging to be on before it can be entered, so a page table
; has to exist before there is any C++ to build one with. What is set up here is
; the simplest thing that works: the first 4 GiB identity mapped with 2 MiB
; pages. Identity mapping keeps every physical address the boot loader gave us
; valid, and 4 GiB is enough to reach the memory mapped APIC registers.

[BITS 32]

[EXTERN GdtLongModePointer]  ; Arch/Gdt.cpp
[EXTERN KernelInit]          ; Boot/Startup.cpp

[SECTION .text]
[GLOBAL LongMode]
LongMode:

CPUID_SUPPORTED_BIT equ 1 << 21

CheckCpuidSupported:
    ; There is no feature bit that says "cpuid exists" -- the test is whether the
    ; ID flag in EFLAGS can be toggled at all.
    pushfd
    mov eax, [esp]
    xor eax, CPUID_SUPPORTED_BIT
    push eax
    popfd
    pushfd
    pop eax
    ; Compare against the original EFLAGS: a set bit here means it stuck.
    xor eax, [esp]
    popfd
    and eax, CPUID_SUPPORTED_BIT
    jnz CheckLongMode

    ; "No CPUID", white on red, written straight into the text framebuffer.
    mov dword [0xb8000], 0xcf6fcf4e
    mov dword [0xb8004], 0xcf43cf20
    mov dword [0xb8008], 0xcf55cf50
    mov dword [0xb800c], 0xcf44cf49
    hlt

CPUID_EXTENDED_MAX      equ 0x80000000
CPUID_EXTENDED_FEATURES equ 0x80000001
CPUID_LONG_MODE_BIT     equ 1 << 29

CheckLongMode:
    ; Long mode is reported by an extended cpuid leaf, so first ask whether the
    ; extended leaves exist at all.
    mov eax, CPUID_EXTENDED_MAX
    cpuid
    cmp eax, CPUID_EXTENDED_FEATURES
    jb .NoLongMode

    mov eax, CPUID_EXTENDED_FEATURES
    cpuid
    test edx, CPUID_LONG_MODE_BIT
    jnz SetupPaging

.NoLongMode:
    ; "No 64bit"
    mov dword [0xb8000], 0xcf6fcf4e
    mov dword [0xb8004], 0xcf36cf20
    mov dword [0xb8008], 0xcf62cf34
    mov dword [0xb800c], 0xcf74cf69
    hlt

PAGE_SIZE           equ 4096
PAGE_PRESENT        equ 1 << 0
PAGE_WRITEABLE      equ 1 << 1
PAGE_HUGE           equ 1 << 7   ; 2 MiB page at level 2, no level 1 table needed

SetupPaging:
    ; Long mode paging is four levels deep with 512 eight-byte entries each:
    ;   PML4 entry -> 512 GiB   PDPT entry -> 1 GiB   PD entry (huge) -> 2 MiB
    ; 4 GiB therefore needs one PML4, one PDPT and four page directories.

    ; Fill the four page directories with 2048 consecutive 2 MiB pages.
    mov ecx, 0
.MapDirectories:
    mov eax, 0x200000
    mul ecx
    or eax, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_HUGE
    mov [PagingLevel2Tables + ecx * 8], eax
    inc ecx
    cmp ecx, 512 * 4
    jne .MapDirectories

    ; Point the first four PDPT entries at those directories.
    mov ecx, 0
.MapDirectoryPointers:
    mov eax, PAGE_SIZE
    mul ecx
    add eax, PagingLevel2Tables
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel3Table + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .MapDirectoryPointers

    ; And the first PML4 entry at the PDPT.
    mov eax, PagingLevel3Table
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel4Table], eax

EnablePaging:
    mov eax, PagingLevel4Table
    mov cr3, eax

    ; Physical Address Extension. Long mode page tables are PAE format, so this
    ; has to be on before the mode switch.
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Long Mode Enable in the EFER model specific register.
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Setting the paging bit with LME already set is what activates long mode.
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; The CPU is now in compatibility mode; a far jump through a descriptor with
    ; the L bit set is what finally gets us into 64-bit code.
    lgdt [GdtLongModePointer]
    jmp 0x08:LongModeStart

[SECTION .bss]
align 4096

[GLOBAL PagingLevel4Table]
[GLOBAL PagingLevel3Table]
[GLOBAL PagingLevel2Tables]
PagingLevel4Table:
    resb PAGE_SIZE
PagingLevel3Table:
    resb PAGE_SIZE
PagingLevel2Tables:
    resb PAGE_SIZE * 4

[SECTION .text]
[BITS 64]

LongModeStart:
    ; Segmentation is gone; the data segment registers are ignored and are only
    ; cleared so nothing stale is left behind.
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call KernelInit

    ; "STOP" -- KernelInit is not supposed to return.
    mov rax, 0x2f502f4f2f544f53
    mov qword [abs 0xb8000], rax
.Halt:
    cli
    hlt
    jmp .Halt
