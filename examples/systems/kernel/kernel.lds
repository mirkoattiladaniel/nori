/* kernel.lds — memory layout for a --freestanding Nori kernel.
 * Loaded at 1 MiB (multiboot). The __mb_* symbols feed the a.out-kludge fields in boot.S;
 * .bss stays out of the flat binary and is zeroed by the loader via bss_end_addr.
 * The bare floor's bump heap lives above the image, at [16 MiB, 80 MiB) (rt_shim_bare.c). */
ENTRY(_start)
SECTIONS {
    . = 0x100000;
    __mb_load_start = .;
    .mbheader : { *(.mbheader) }
    .text     : { *(.text*) }
    .rodata   : { *(.rodata*) }
    .data     : { *(.data*) }
    __mb_load_end = .;
    .bss      : { *(COMMON) *(.bss*) }
    . = ALIGN(8);
    __mb_bss_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
