KERNEL_VMA = 0xFFFFFFFF80000000;

ENTRY(StartupBsp)

SECTIONS
{
    /* 16 MiB keeps the kernel clear of the legacy sub-1 MiB clutter and of the
       region boot loaders like to drop modules into. */
    . = 16M;
    ___KERNEL_START___ = .;

    /* Each of the three spans below is mapped with its own page permissions, so
       every boundary has to land on a page. */
    /* The 32-bit boot path runs before anything maps the higher half, so it
       stays where the loader puts it. */
    ___BOOT_START___ = .;
    .boot ALIGN(4096) :
    {
        KEEP(*(.multiboot_header))
        *(.boot)
        *(.boot.*)
    }
    . = ALIGN(4096);
    ___BOOT_END___ = .;

    . += KERNEL_VMA;

    ___KERNEL_TEXT_START___ = .;
    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }

    . = ALIGN(4096);
    ___KERNEL_TEXT_END___ = .;

    ___KERNEL_RODATA_START___ = .;
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Constructor tables emitted by the compiler for global objects. */
    .init_array : AT(ADDR(.init_array) - KERNEL_VMA)
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);

        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .fini_array : AT(ADDR(.fini_array) - KERNEL_VMA)
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }

    . = ALIGN(4096);
    ___KERNEL_RODATA_END___ = .;

    ___KERNEL_DATA_START___ = .;
    .data : AT(ADDR(.data) - KERNEL_VMA)
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
    .ap_trampoline ALIGN(16) : AT(ADDR(.ap_trampoline) - KERNEL_VMA)
    {
        ___AP_TRAMPOLINE_START___ = .;
        *(.ap_trampoline)
    }
    ___AP_TRAMPOLINE_END___ = .;

    .bss : AT(ADDR(.bss) - KERNEL_VMA)
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    . = ALIGN(4096);
    ___KERNEL_DATA_END___ = .;
    ___KERNEL_END___ = .;

    /* The 32-bit boot path runs before the higher half exists, so it reaches
       these through their load addresses. */
    MultibootMagicPhys = MultibootMagic - KERNEL_VMA;
    MultibootInfoPhys = MultibootInfo - KERNEL_VMA;
    CoreStackSizePhys = CoreStackSize - KERNEL_VMA;
    CoreStackPointerPhys = CoreStackPointer - KERNEL_VMA;
    KernelPageTableRootPhys = KernelPageTableRoot - KERNEL_VMA;
    PagingLevel4TablePhys = PagingLevel4Table - KERNEL_VMA;
    PagingLevel3TablePhys = PagingLevel3Table - KERNEL_VMA;
    PagingLevel3TableHighPhys = PagingLevel3TableHigh - KERNEL_VMA;
    PagingLevel2TablesPhys = PagingLevel2Tables - KERNEL_VMA;

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
