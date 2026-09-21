ENTRY(StartupBsp)

SECTIONS
{
    /* 16 MiB keeps the kernel clear of the legacy sub-1 MiB clutter and of the
       region boot loaders like to drop modules into. */
    . = 16M;
    ___KERNEL_START___ = .;

    /* The Multiboot headers have to be near the very start of the image. */
    .boot :
    {
        KEEP(*(.multiboot_header))
    }

    ___KERNEL_TEXT_START___ = .;
    .text :
    {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Constructor tables emitted by the compiler for global objects. */
    .init_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);

        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }
    ___KERNEL_TEXT_END___ = .;

    .data :
    {
        *(.data)
        *(.data.*)
        *(.got)
        *(.got.plt)
    }

    /* Real mode startup code for the application processors.
       A linker could place this below 1 MiB directly, but an ELF with two such
       distant load segments trips up QEMU's own Multiboot loader, so it is
       linked here and copied down at runtime instead. */
    .ap_trampoline ALIGN(16) :
    {
        ___AP_TRAMPOLINE_START___ = .;
        *(.ap_trampoline)
    }
    ___AP_TRAMPOLINE_END___ = .;

    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    ___KERNEL_END___ = .;

    /DISCARD/ :
    {
        *(.interp)
        *(.note)
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
