; src/isr.asm - Final Fixed Version

[BITS 32]

; --- External C Handlers ---
extern keyboard_handler
extern schedule_and_eoi  

; --- Assembly System Functions (Exported to C via global) ---
global irq0
global irq1
global inb
global outb
global load_cr3
global enable_paging


; --------------------------------------
; --- INTERRUPT STUBS (IRQ 0 and IRQ 1) ---
; --------------------------------------

; --- Timer (IRQ 0) Stub ---
irq0:
    pushad
    call schedule_and_eoi  ; Calls the C function (updates clock, sends EOI)
    popad
    iret 

    
; --- Keyboard (IRQ 1) Stub ---
irq1:
    pushad
    call keyboard_handler
    popad
    iret 
    
; --------------------------------------
; --- SYSTEM FUNCTIONS ---
; --------------------------------------

inb:
    mov dx, [esp+4] 
    in al, dx
    ret

outb:
    mov dx, [esp+4]   
    mov al, [esp+8]   
    out dx, al
    ret
    
load_cr3:
    mov eax, [esp+4] 
    mov cr3, eax     
    ret

enable_paging:
    mov eax, cr0
    or eax, 0x80000000 
    or eax, 0x00000001 
    mov cr0, eax
    ret
