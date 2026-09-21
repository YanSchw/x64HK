; Multiboot constants shared between the header and the entry code.

; --- Multiboot2 (the one we actually target) ---------------------------------
MULTIBOOT2_HEADER_MAGIC     equ 0xe85250d6
MULTIBOOT2_LOADER_MAGIC     equ 0x36d76289   ; handed to us in eax
MULTIBOOT2_ARCHITECTURE_I386 equ 0

MULTIBOOT2_TAG_END              equ 0
MULTIBOOT2_TAG_INFORMATION_REQ  equ 1
MULTIBOOT2_TAG_CONSOLE_FLAGS    equ 4
MULTIBOOT2_TAG_FRAMEBUFFER      equ 5
MULTIBOOT2_TAG_MODULE_ALIGN     equ 6

MULTIBOOT2_TAG_OPTIONAL         equ 1        ; boot loader may ignore the tag

; Console flags: bit 1 promises that the kernel can drive EGA text mode, which
; is what stops GRUB from switching to a linear framebuffer we have no driver for.
MULTIBOOT2_CONSOLE_EGA_TEXT     equ 1 << 1

; Information request tag entries (boot info tag types).
MULTIBOOT2_INFO_CMDLINE         equ 1
MULTIBOOT2_INFO_BOOT_LOADER     equ 2
MULTIBOOT2_INFO_MODULE          equ 3
MULTIBOOT2_INFO_BASIC_MEMINFO   equ 4
MULTIBOOT2_INFO_MEMORY_MAP      equ 6
MULTIBOOT2_INFO_FRAMEBUFFER     equ 8
MULTIBOOT2_INFO_ACPI_OLD        equ 14
MULTIBOOT2_INFO_ACPI_NEW        equ 15

; --- Multiboot1 (kept only so `qemu -kernel` still boots the raw ELF) ---------
MULTIBOOT1_HEADER_MAGIC     equ 0x1badb002
MULTIBOOT1_LOADER_MAGIC     equ 0x2badb002   ; handed to us in eax
MULTIBOOT1_PAGE_ALIGN       equ 1 << 0
MULTIBOOT1_MEMORY_INFO      equ 1 << 1
MULTIBOOT1_HEADER_FLAGS     equ MULTIBOOT1_PAGE_ALIGN | MULTIBOOT1_MEMORY_INFO
MULTIBOOT1_HEADER_CHECKSUM  equ -(MULTIBOOT1_HEADER_MAGIC + MULTIBOOT1_HEADER_FLAGS)
