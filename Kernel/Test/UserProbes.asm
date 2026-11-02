; Ring 3 probes for Test/UserModeTest.cpp.
;
; Each one is a few instructions that the suite copies into a user page and
; runs as a task of its own. They are position independent -- absolute
; immediates and rip-relative addressing only -- because they execute from
; wherever the suite maps them, not from where they are linked.
;
; Anything that must fault is followed by ud2, so an instruction that wrongly
; succeeds reports an invalid opcode instead of passing quietly.

[SECTION .rodata]

[BITS 64]

SYS_EXIT    equ 0
SYS_WRITE   equ 1
SYS_YIELD   equ 2
SYS_GET_KEY equ 3

; Two kernel addresses that really are mapped, so the probes below test the
; U/S bit rather than an address that happens to be absent. Config.h owns the
; direct map base; the linker script owns the other.
[EXTERN ___KERNEL_TEXT_START___]
DIRECT_MAP_ADDRESS equ 0xFFFF800000001000

%macro PROBE 1
    [GLOBAL Probe%1Start]
    [GLOBAL Probe%1End]
    align 16
    Probe%1Start:
%endmacro

; --- Well behaved -----------------------------------------------------------

; Exit(7). The whole round trip: syscall in, dispatch, task taken down.
PROBE Exit
    mov eax, SYS_EXIT
    mov edi, 7
    xor esi, esi
    xor edx, edx
    syscall
    ud2
ProbeExitEnd:

; Yield returns normally, which is what makes this probe's untouched stack mean
; something: the suite checks afterwards that the kernel never wrote below the
; user rsp, and this program never pushes anything itself.
PROBE Yield
    mov eax, SYS_YIELD
    xor edi, edi
    xor esi, esi
    xor edx, edx
    syscall

    mov edi, eax
    mov eax, SYS_EXIT
    xor esi, esi
    xor edx, edx
    syscall
    ud2
ProbeYieldEnd:

; Blocks until the suite hands the Vault a keystroke, then exits with it.
PROBE GetKey
    mov eax, SYS_GET_KEY
    xor edi, edi
    xor esi, esi
    xor edx, edx
    syscall

    mov edi, eax
    mov eax, SYS_EXIT
    xor esi, esi
    xor edx, edx
    syscall
    ud2
ProbeGetKeyEnd:

; An unknown call number has to come back as an error, not a fault. The result
; is handed to Exit, so the suite reads it as the status.
PROBE UnknownCall
    mov eax, 99
    xor edi, edi
    xor esi, esi
    xor edx, edx
    syscall

    mov edi, eax
    mov eax, SYS_EXIT
    xor esi, esi
    xor edx, edx
    syscall
    ud2
ProbeUnknownCallEnd:

; Write() pointed at kernel memory. The kernel must refuse rather than read it.
PROBE KernelWrite
    mov eax, SYS_WRITE
    mov edi, 1
    mov rsi, ___KERNEL_TEXT_START___
    mov edx, 4
    syscall

    mov edi, eax
    mov eax, SYS_EXIT
    xor esi, esi
    xor edx, edx
    syscall
    ud2
ProbeKernelWriteEnd:

; A reserved argument that is not zero. Same, reported through the status.
PROBE ReservedArgument
    mov eax, SYS_EXIT
    mov edi, 1
    mov esi, 1              ; reserved, has to be zero
    xor edx, edx
    syscall

    mov edi, eax
    mov eax, SYS_EXIT
    xor esi, esi
    xor edx, edx
    syscall
    ud2
ProbeReservedArgumentEnd:

; --- Privileged instructions ------------------------------------------------

PROBE Cli
    cli
    ud2
ProbeCliEnd:

PROBE Hlt
    hlt
    ud2
ProbeHltEnd:

; Port 0x80 is the BIOS POST diagnostic port, harmless if this ever got through.
PROBE Out
    mov dx, 0x80
    xor eax, eax
    out dx, al
    ud2
ProbeOutEnd:

; Every gate in the IDT has DPL 0, so any int from ring 3 is a protection fault.
PROBE Interrupt
    int 0x80
    ud2
ProbeInterruptEnd:

; --- Memory -----------------------------------------------------------------

PROBE KernelRead
    mov rax, ___KERNEL_TEXT_START___
    mov rax, [rax]
    ud2
ProbeKernelReadEnd:

; The direct map is the whole of RAM, so this is the one that matters most.
PROBE DirectMapRead
    mov rax, DIRECT_MAP_ADDRESS
    mov rax, [rax]
    ud2
ProbeDirectMapReadEnd:

; Its own code page, which is mapped read-execute.
PROBE WriteText
    mov byte [rel ProbeWriteTextTarget], 0
ProbeWriteTextTarget:
    ud2
ProbeWriteTextEnd:

; The stack is mapped no-execute, so fetching from it faults.
PROBE ExecuteStack
    lea rax, [rsp - 8]
    jmp rax
ProbeExecuteStackEnd:

; --- Arithmetic -------------------------------------------------------------

PROBE Divide
    xor edx, edx
    xor eax, eax
    div eax
    ud2
ProbeDivideEnd:
