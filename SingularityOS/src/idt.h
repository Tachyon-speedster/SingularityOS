// src/idt.h

// Function to load the IDT register (defined in boot.asm)
extern void load_idt(void*); 

// Functions for I/O port manipulation
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
