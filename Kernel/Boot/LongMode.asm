; Protected mode (32-bit) to long mode (64-bit).
;
; Long mode requires paging to be on before it can be entered, so a page table
; has to exist before there is any C++ to build one with. What is set up here is
; the simplest thing that works: the first 4 GiB identity mapped with 2 MiB
; pages. Identity mapping keeps every physical address the boot loader gave us
; valid, and 4 GiB is enough to reach the memory mapped APIC registers.

[BITS 32]

[EXTERN KernelInit]               ; Boot/Startup.cpp
[EXTERN KernelPageTableRootPhys]  ; Memory/Paging.cpp, through Boot/Sections.ld

; The tables below are linked into the higher half but written here, before
; anything maps it. Boot/Sections.ld exports their load addresses.
[EXTERN PagingLevel4TablePhys]
[EXTERN PagingLevel3TablePhys]
[EXTERN PagingLevel3TableHighPhys]
[EXTERN PagingLevel2TablesPhys]

BOOT_CODE64 equ 0x08

; Config::KERNEL_VMA.
KERNEL_VMA equ 0xffffffff80000000

[SECTION .boot]
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
CPUID_NX_BIT            equ 1 << 20
CPUID_LONG_MODE_BIT     equ 1 << 29

EFER_LME equ 1 << 8
EFER_NXE equ 1 << 11

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
    ; The bootstrap processor arrives here before there is a kernel to ask, so
    ; it builds the map below. Every later core finds the real tables already
    ; published and loads those instead.
    mov eax, [KernelPageTableRootPhys]
    test eax, eax
    jnz EnablePaging

    ; Long mode paging is four levels deep with 512 eight-byte entries each:
    ;   PML4 entry -> 512 GiB   PDPT entry -> 1 GiB   PD entry (huge) -> 2 MiB
    ; 4 GiB therefore needs one PML4, one PDPT and four page directories.

    ; Fill the four page directories with 2048 consecutive 2 MiB pages.
    mov ecx, 0
.MapDirectories:
    mov eax, 0x200000
    mul ecx
    or eax, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_HUGE
    mov [PagingLevel2TablesPhys + ecx * 8], eax
    inc ecx
    cmp ecx, 512 * 4
    jne .MapDirectories

    ; Point the first four PDPT entries at those directories.
    mov ecx, 0
.MapDirectoryPointers:
    mov eax, PAGE_SIZE
    mul ecx
    add eax, PagingLevel2TablesPhys
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel3TablePhys + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .MapDirectoryPointers

    ; And the first PML4 entry at the PDPT.
    mov eax, PagingLevel3TablePhys
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel4TablePhys], eax

    ; Config::DIRECT_MAP_BASE is PML4 entry 256, and wants exactly what the
    ; identity map already has, so the same page directory pointer table serves
    ; both. C++ can then reach any frame from the moment it starts running.
    mov eax, PagingLevel3TablePhys
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel4TablePhys + 256 * 8], eax

    ; The kernel is linked into the top 2 GiB, so the same first page directory
    ; is hung there too: 0xffffffff80000000 is PML4 entry 511, PDPT entry 510.
    mov eax, PagingLevel2TablesPhys
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel3TableHighPhys + 510 * 8], eax

    mov eax, PagingLevel3TableHighPhys
    or eax, PAGE_PRESENT | PAGE_WRITEABLE
    mov [PagingLevel4TablePhys + 511 * 8], eax

    mov eax, PagingLevel4TablePhys

; Expects the page table root in eax.
EnablePaging:
    mov cr3, eax

    ; Physical Address Extension. Long mode page tables are PAE format, so this
    ; has to be on before the mode switch.
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; No-execute has to be enabled before any table carrying the bit is used,
    ; or the CPU reads bit 63 as reserved and faults on every mapping. Checked
    ; rather than assumed, because wrmsr of an unsupported bit is a #GP.
    mov eax, CPUID_EXTENDED_FEATURES
    cpuid
    and edx, CPUID_NX_BIT
    mov ebx, edx

    ; Long Mode Enable in the EFER model specific register.
    mov ecx, 0xc0000080
    rdmsr
    or eax, EFER_LME
    test ebx, ebx
    jz .NoNoExecute
    or eax, EFER_NXE
.NoNoExecute:
    wrmsr

    ; Setting the paging bit with LME already set is what activates long mode.
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; The CPU is now in compatibility mode; a far jump through a descriptor with
    ; the L bit set is what finally gets us into 64-bit code. Boot/Entry.asm
    ; already loaded the table this selector comes from.
    jmp BOOT_CODE64:LongModeStart

[SECTION .bss]
align 4096

[GLOBAL PagingLevel4Table]
[GLOBAL PagingLevel3Table]
[GLOBAL PagingLevel3TableHigh]
[GLOBAL PagingLevel2Tables]
PagingLevel4Table:
    resb PAGE_SIZE
PagingLevel3Table:
    resb PAGE_SIZE
PagingLevel3TableHigh:
    resb PAGE_SIZE
PagingLevel2Tables:
    resb PAGE_SIZE * 4

; Still the low section: the far jump above carries a 32-bit offset, so the
; landing site cannot be in the higher half.
[SECTION .boot]
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

    ; Boot/Entry.asm had to pick the stack by its load address, which stops
    ; being mapped the moment Paging drops the loader's one to one map. Nothing
    ; has been pushed yet, so moving to the kernel alias costs nothing.
    mov rax, KERNEL_VMA
    add rsp, rax

    ; KernelInit is in the higher half, far out of reach of a rel32 call.
    mov rax, KernelInit
    call rax

    ; "STOP" -- KernelInit is not supposed to return.
    mov rax, 0x2f502f4f2f544f53
    mov qword [abs 0xb8000], rax
.Halt:
    cli
    hlt
    jmp .Halt
