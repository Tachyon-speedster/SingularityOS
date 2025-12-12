; src/isr.asm

[BITS 32]

; --- Keyboard (IRQ 1) Stub ---
global irq1

irq1:
    ; 1. Save all registers (context switching)
    pushad
    
    ; 2. Call the C keyboard handler
    extern keyboard_handler
    call keyboard_handler
    
    ; 3. Restore all registers
    popad
    
    ; 4. Return from interrupt
    iret 
    
; --- I/O Port Functions (inb/outb) ---
; These are wrapper functions to let the C code talk to I/O ports

global inb
inb:
    ; Read byte from port specified in AL
    mov dx, [esp+4] ; Load port number from argument (pushed onto stack)
    in al, dx
    ret

global outb
outb:
    ; Write byte (second argument) to port (first argument)
    mov dx, [esp+4]   ; Load port
    mov al, [esp+8]   ; Load data
    out dx, al
    ret
    
; src/isr.asm (Add these functions to the end of the file)

; --- Paging Functions ---

global load_cr3
load_cr3:
    ; Function to load the Page Directory Base Register (CR3)
    mov eax, [esp+4] ; Load the address of the Page Directory (first argument)
    mov cr3, eax     ; Set CR3
    ret

global enable_paging
enable_paging:
    ; Function to enable Paging
    ; Set the PG (Paging) bit and the PE (Protection Enable) bit in CR0
    mov eax, cr0
    or eax, 0x80000000 ; PG bit (bit 31)
    or eax, 0x00000001 ; PE bit (bit 0)
    mov cr0, eax
    ret    
