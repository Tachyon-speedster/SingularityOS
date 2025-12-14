; src/boot.asm

[BITS 32]

; --- Multiboot Header (Required by GRUB) ---
section .multiboot
    align 4
    ; Magic number
    dd 0x1BADB002
    ; Flags (we want page alignment and memory info)
    dd 0x00
    ; Checksum (Magic + Flags + Checksum = 0)
    dd -(0x1BADB002 + 0x00)

; --- BSS Section (Uninitialized Data) ---
section .bss
    align 4
    ; A simple 16KB stack space
    stack_bottom:
    resb 16384
    stack_top:

; --- Code Section ---
section .text
    ; Must be a global symbol so the linker can find it
    global _start
    ; The C kernel entry point (defined in kernel.c)
    extern kernel_main

_start:
    ; Set up the stack pointer (ESP) to point to the top of our stack space
    mov esp, stack_top
    
    ; Call the C kernel entry point
    call kernel_main
    
    ; Halt the CPU (infinite loop)
.loop:
    hlt
    jmp .loop

; src/boot.asm (Add these lines at the very bottom)
global load_idt

load_idt:
    mov eax, [esp+4]    ; IDT register address is passed as argument
    lidt [eax]          ; Load the IDT register
    ret
