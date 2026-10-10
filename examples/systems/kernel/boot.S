/* boot.S — multiboot entry + 32->64-bit long-mode trampoline for a Nori kernel.
 *
 * QEMU (`-kernel kernel.bin`) or GRUB loads us at 1 MiB via the multiboot v1 a.out kludge
 * (the flat --flatbin image; multiboot loaders don't read ELF64). We arrive in 32-bit
 * protected mode with paging off, and everything Nori needs is set up here:
 *   identity-map the first 1 GiB with 2 MiB pages -> enable PAE + long mode -> load a
 *   64-bit GDT -> enable SSE (Nori Float is f64; clang emits SSE2) -> call nori_kentry.
 */

/* ---- multiboot v1 header (a.out kludge: bit 16 tells the loader the addresses) ---- */
.section .mbheader, "a"
.align 4
mb_header:
    .long 0x1BADB002                        /* magic */
    .long 0x00010003                        /* flags: kludge | meminfo | page-align */
    .long -(0x1BADB002 + 0x00010003)        /* checksum */
    .long mb_header                         /* header_addr */
    .long __mb_load_start                   /* load_addr */
    .long __mb_load_end                     /* load_end_addr */
    .long __mb_bss_end                      /* bss_end_addr (loader zeroes our .bss) */
    .long _start                            /* entry_addr */

/* ---- 32-bit entry ---- */
.section .text
.code32
.global _start
_start:
    cli
    movl $stack_top, %esp

    /* page tables: pml4[0] -> pdpt, pdpt[0] -> pd, pd[i] = i*2MiB | present|write|huge */
    movl $pdpt, %eax
    orl  $0x3, %eax
    movl %eax, pml4
    movl $pd, %eax
    orl  $0x3, %eax
    movl %eax, pdpt
    movl $0, %ecx
    movl $pd, %edi
1:  movl %ecx, %eax
    shll $21, %eax                          /* i * 2 MiB (< 1 GiB, fits in 32 bits) */
    orl  $0x83, %eax                        /* present | writable | 2MiB page */
    movl %eax, (%edi)
    movl $0, 4(%edi)
    addl $8, %edi
    incl %ecx
    cmpl $512, %ecx
    jne  1b

    movl %cr4, %eax                         /* PAE on */
    orl  $0x20, %eax
    movl %eax, %cr4
    movl $pml4, %eax                        /* root table */
    movl %eax, %cr3
    movl $0xC0000080, %ecx                  /* EFER.LME: long mode enable */
    rdmsr
    orl  $0x100, %eax
    wrmsr
    movl %cr0, %eax                         /* paging on -> now in 32-bit compatibility mode */
    orl  $0x80000000, %eax
    movl %eax, %cr0
    lgdt gdt_ptr
    ljmp $0x08, $long_mode                  /* far jump into the 64-bit code segment */

/* ---- 64-bit ---- */
.code64
long_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    movq $stack_top, %rsp

    movq %cr0, %rax                         /* SSE: clear EM, set MP ... */
    andq $~0x4, %rax
    orq  $0x2, %rax
    movq %rax, %cr0
    movq %cr4, %rax                         /* ... and OSFXSR | OSXMMEXCPT */
    orq  $0x600, %rax
    movq %rax, %cr4

    call nori_kentry                        /* the bare C floor: serial up, then nori_main() */
2:  cli
    hlt
    jmp 2b

/* ---- GDT: null, 64-bit code, data ---- */
.section .rodata
.align 8
gdt:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
gdt_ptr:
    .word gdt_ptr - gdt - 1
    .long gdt

/* ---- zero-filled by the loader (bss_end_addr above) ---- */
.section .bss
.align 4096
pml4:  .skip 4096
pdpt:  .skip 4096
pd:    .skip 4096
.align 16
stack:     .skip 262144                     /* 256 KiB kernel stack */
stack_top:
