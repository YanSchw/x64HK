; Low level half of the context switch.
;
; This has to be assembly: the switch changes RSP mid-function, so the compiler
; would have no consistent view of the frame, and the register save/restore has
; to be exactly the set that Context declares.
;
; Context layout (see Arch/Context.h):
;   +0 rbx   +8 rbp   +16 r12   +24 r13   +32 r14   +40 r15   +48 rsp

[SECTION .text]

[GLOBAL ContextSwitch]
[GLOBAL ContextLaunch]
[GLOBAL FakeSystemVAbi]

%macro SAVE_CONTEXT 0
    mov [rsi +  0], rbx
    mov [rsi +  8], rbp
    mov [rsi + 16], r12
    mov [rsi + 24], r13
    mov [rsi + 32], r14
    mov [rsi + 40], r15
    mov [rsi + 48], rsp
%endmacro

%macro LOAD_CONTEXT 0
    mov rbx, [rdi +  0]
    mov rbp, [rdi +  8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 32]
    mov r15, [rdi + 40]
    mov rsp, [rdi + 48]
%endmacro

; void ContextSwitch(Context* InNext /* rdi */, Context* OutCurrent /* rsi */)
align 16
ContextSwitch:
    SAVE_CONTEXT
    LOAD_CONTEXT
    ; Returns onto the new stack: either back into a previous ContextSwitch
    ; call, or into FakeSystemVAbi for a thread that has never run.
    ret

; void ContextLaunch(Context* InNext /* rdi */)
align 16
ContextLaunch:
    LOAD_CONTEXT
    ret

; The first activation of a thread only restores callee-saved registers, but the
; SysV ABI passes arguments in rdi/rsi/rdx. PrepareContext parked the kickoff
; arguments in r13/r14/r15, so move them across before falling through into the
; kickoff function via the return address below.
align 16
FakeSystemVAbi:
    mov rdi, r13
    mov rsi, r14
    mov rdx, r15
    ret
