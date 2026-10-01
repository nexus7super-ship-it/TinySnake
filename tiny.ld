/* Linker script for the ultra-trimmed build of TinySnake.
 *
 * It does three things that the default script doesn't:
 * - It leaves out the PT_PHDR program header, which only position
 *   independent executables need.
 * - It puts the data segment right behind the code in the file, without
 *   padding it to a page boundary.
 * - It ends the file with the parts that are all zero bytes (the end of
 *   .dynamic and the GOT, which the dynamic loader fills in), so that the
 *   Makefile can cut them off. The kernel fills the rest of the page with
 *   zeros anyway.
 */

ENTRY(_start)

PHDRS
{
    interp  PT_INTERP;
    text    PT_LOAD FILEHDR PHDRS FLAGS(5);  /* readable, executable */
    data    PT_LOAD FLAGS(6);                /* readable, writable */
    dynamic PT_DYNAMIC;
    stack   PT_GNU_STACK FLAGS(6);           /* the stack isn't executable */
}

SECTIONS
{
    /* Sorted by alignment, so that little padding is needed between them. */
    . = 0x400000 + SIZEOF_HEADERS;
    .rodata   : { *(.rodata .rodata.*) } :text
    .dynsym   : { *(.dynsym) }
    .rela.dyn : { *(.rela.*) }
    .gnu.hash : { *(.gnu.hash) }
    .interp   : { *(.interp) } :text :interp
    .dynstr   : { *(.dynstr) } :text
    .text     : { *(.text .text.*) }

    /* Same position within the page as in the file, one page higher: so the
     * data segment needs no padding in the file. */
    . += 0x1000;
    .data     : { *(.data .data.*) *(.got.plt) } :data
    .dynamic  : { *(.dynamic) } :data :dynamic
    .got      : { *(.got) } :data
    .bss      : { *(.bss .bss.*) *(COMMON) } :data

    /DISCARD/ : { *(.note.*) *(.comment) *(.eh_frame*) }
}
