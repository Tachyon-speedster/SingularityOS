/* src/linker.ld */

OUTPUT_FORMAT(elf32-i386)    /* Output an i386 ELF file */
ENTRY(_start)                /* Set the entry point to the _start label in boot.asm */

SECTIONS
{
    /* The kernel will be loaded by GRUB at 1 Megabyte (1MB) */
    . = 0x100000; 

    /* .text section: Code */
    .text :
    {
        *(.multiboot)   /* Place the multiboot header first */
        *(.text)
    }

    /* .data section: Initialized data */
    .data :
    {
        *(.data)
    }

    /* .bss section: Uninitialized data (cleared to 0 by GRUB/boot code later) */
    .bss :
    {
        *(.bss)
    }
}
