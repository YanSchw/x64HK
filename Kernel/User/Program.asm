; The ring 3 demo program, embedded in the kernel image.
;
; Tools/User.mk builds it and puts its directory on the assembler's include
; path, which is what lets incbin find it by name. There is no filesystem yet,
; so this is how a user program reaches the kernel at all.

[SECTION .rodata]

[GLOBAL UserProgramStart]
[GLOBAL UserProgramEnd]

align 16
UserProgramStart:
    incbin "Hello.elf"
UserProgramEnd:
