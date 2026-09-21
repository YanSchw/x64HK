; Boot loader handshake.
;
; A Multiboot compliant loader scans the start of the kernel image for a magic
; header and reads the tags that follow it to decide how to load us. The linker
; script puts this section first so both headers land inside the window the
; specs require (32 KiB for Multiboot2, 8 KiB for Multiboot1).
;
; Two headers are present on purpose. GRUB uses the Multiboot2 one, which is the
; only way to get the ACPI RSDP and a proper memory map handed over. QEMU's
; built-in `-kernel` loader only understands Multiboot1, and keeping that path
; alive is what makes `make qemu` a one second edit-test cycle.

%include "Boot/Multiboot.inc"

[SECTION .multiboot_header]

align 8
Multiboot2Header:
    dd MULTIBOOT2_HEADER_MAGIC
    dd MULTIBOOT2_ARCHITECTURE_I386
    dd Multiboot2HeaderEnd - Multiboot2Header
    dd -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCHITECTURE_I386 + (Multiboot2HeaderEnd - Multiboot2Header))

    ; Ask for the boot information we care about. Marked optional so a loader
    ; that cannot supply one of them still boots us instead of refusing.
align 8
.InformationRequest:
    dw MULTIBOOT2_TAG_INFORMATION_REQ
    dw MULTIBOOT2_TAG_OPTIONAL
    dd .InformationRequestEnd - .InformationRequest
    dd MULTIBOOT2_INFO_CMDLINE
    dd MULTIBOOT2_INFO_BOOT_LOADER
    dd MULTIBOOT2_INFO_MODULE
    dd MULTIBOOT2_INFO_BASIC_MEMINFO
    dd MULTIBOOT2_INFO_MEMORY_MAP
    dd MULTIBOOT2_INFO_FRAMEBUFFER
    dd MULTIBOOT2_INFO_ACPI_OLD
    dd MULTIBOOT2_INFO_ACPI_NEW
.InformationRequestEnd:

    ; Keep us in text mode. Without this GRUB is free to hand over a linear
    ; framebuffer, and 0xb8000 would then no longer be the screen.
align 8
.ConsoleFlags:
    dw MULTIBOOT2_TAG_CONSOLE_FLAGS
    dw 0
    dd .ConsoleFlagsEnd - .ConsoleFlags
    dd MULTIBOOT2_CONSOLE_EGA_TEXT
.ConsoleFlagsEnd:

    ; Page align boot modules so an initrd can be mapped directly.
align 8
.ModuleAlign:
    dw MULTIBOOT2_TAG_MODULE_ALIGN
    dw 0
    dd 8

align 8
.End:
    dw MULTIBOOT2_TAG_END
    dw 0
    dd 8
Multiboot2HeaderEnd:

align 4
Multiboot1Header:
    dd MULTIBOOT1_HEADER_MAGIC
    dd MULTIBOOT1_HEADER_FLAGS
    dd MULTIBOOT1_HEADER_CHECKSUM
