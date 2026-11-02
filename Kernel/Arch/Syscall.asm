; System call entry and the one-way trip into ring 3.
;
; `syscall` is not an interrupt: it switches no stack, pushes nothing, and hands
; over rip in rcx and rflags in r11. It arrives with rsp still pointing into user
; memory, so the first thing that has to happen is a switch to this core's kernel
; stack -- which is reached through KERNEL_GS_BASE, the only thing swapgs is used
; for in this kernel.

[SECTION .text]

[GLOBAL SyscallEntry]
[GLOBAL EnterUserMode]
[EXTERN SyscallDispatch]

; Arch/Syscall.h::Area
AREA_KERNEL_STACK equ 0
AREA_USER_STACK   equ 8

; Arch/Gdt.h::Selector, with the ring 3 privilege level or'd in
USER_CODE equ 0x28 | 3
USER_DATA equ 0x20 | 3

; RFLAGS a fresh user thread starts with: reserved bit 1, interrupts enabled.
USER_FLAGS equ 0x202

align 16
SyscallEntry:
    ; The scratch slot is per core, not per thread, so the parked stack pointer
    ; is moved onto the kernel stack immediately and gs put back. SFMASK cleared
    ; IF, so nothing can interrupt the four instructions in between -- and from
    ; the second swapgs onwards this core is back to "gs base is the user's",
    ; which is what lets the body block and be preempted.
    swapgs
    mov [gs:AREA_USER_STACK], rsp
    mov rsp, [gs:AREA_KERNEL_STACK]
    push qword [gs:AREA_USER_STACK]
    swapgs

    push r11                ; user rflags, for sysret
    push rcx                ; user rip, for sysret

    push r10                ; caller saved registers the ABI promises to keep
    push r9
    push r8
    push rdx
    push rsi
    push rdi
    sub rsp, 8              ; SysV wants rsp 16 byte aligned at the call

    ; (rax, rdi, rsi, rdx) -> (rdi, rsi, rdx, rcx)
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax

    sti                     ; a system call may block; rcx and r11 are saved
    call SyscallDispatch
    cli

    add rsp, 8
    pop rdi
    pop rsi
    pop rdx
    pop r8
    pop r9
    pop r10
    pop rcx
    pop r11
    pop rsp
    o64 sysret

; [[noreturn]] void EnterUserMode(uintptr_t InEntry /* rdi */, uintptr_t InStackTop /* rsi */)
;
; iretq rather than sysret: it takes the whole target state off the stack, so
; there is no dependence on rcx and r11 already holding the right thing.
;
; gs is left alone on purpose. Outside the four instructions above, this core's
; gs base is always zero and KERNEL_GS_BASE always points at the Area, so a
; thread entering ring 3 for the first time already satisfies that invariant.
align 16
EnterUserMode:
    mov ax, USER_DATA
    mov ds, ax
    mov es, ax

    push qword USER_DATA
    push rsi
    push qword USER_FLAGS
    push qword USER_CODE
    push rdi

    ; Nothing may leak into ring 3, so every register the program has no use for
    ; is cleared. rdi and rsi are already on the stack by now.
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    iretq
