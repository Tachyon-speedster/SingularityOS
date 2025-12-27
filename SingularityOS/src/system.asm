section .text

; --- 1. Port I/O (inb) ---
; C prototype: unsigned char inb(unsigned short port);
global inb
inb:
    mov edx, [esp+4]    ; Get the port number
    in al, dx           ; Read byte from port (DX) into AL
    movzx eax, al       ; Zero-extend AL into EAX for return
    ret


; --- 2. Port I/O (outb) ---
; C prototype: void outb(unsigned short port, unsigned char data);
global outb
outb:
    mov edx, [esp+4]    ; Get port number
    mov eax, [esp+8]    ; Get data byte
    out dx, al          ; Write AL (data) to port (DX)
    ret

; --- 3. Paging Control (load_cr3) ---
; C prototype: void load_cr3(unsigned int page_directory_base);
global load_cr3
load_cr3:
    mov eax, [esp+4]    ; Get the address
    mov cr3, eax        ; Load the address into CR3
    ret

; --- 4. Paging Control (enable_paging) ---
; C prototype: void enable_paging();
global enable_paging
enable_paging:
    mov eax, cr0        
    or eax, 0x80000000  ; Set the Paging Enable (PG) bit
    mov cr0, eax        
    ret
   

; --- 5. Context Switching (switch_context) ---
; C prototype: void switch_context(TaskContext* old_context, TaskContext* new_context);
global switch_context
switch_context:
    
    ; 1. SAVE the CURRENT context (into *old_context)
    
    ; a) Save current ESP (which holds the start of the saved TaskContext).
    ; Arguments: [esp+4]=RET_ADDR, [esp+8]=&old_context, [esp+12]=&new_context
    mov eax, [esp+8]            ; EAX = &old_context
    mov [eax], esp              ; Save the current ESP (will point to the saved GS register below)

    ; b) Save the general registers (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    pushad                      ; Pushes 8 registers (32 bytes)
    
    ; c) Save segment registers (GS, FS, ES, DS)
    push gs
    push fs
    push es
    push ds
    
    ; The stack now perfectly matches the TaskContext structure,
    ; but starting from the *bottom* (DS is the highest address, GS is the lowest address).
    
    ; 2. LOAD the NEW context (from *new_context)
    
    ; a) Load the new stack pointer
    mov ebx, [esp+56]           ; EBX = &new_context (56 = 4 segs + 8 regs + 4 RET_ADDR + 4 old_context)
    mov esp, [ebx]              ; ESP now points to the saved DS register of the new task

    ; b) Restore segment registers
    pop ds
    pop es
    pop fs
    pop gs
    
    ; c) Restore general registers
    popad                       
    
    ret
