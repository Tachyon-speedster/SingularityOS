// src/kernel.c

// --- Includes ---
#include "idt.h" 
#include "singularity_defs.h" 


// --- Global Constants and Definitions ---
SystemStateBlock g_ssb;
AISCI_Command g_aisci_command;
// PIC I/O Port Definitions 
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

// VGA Output Constants
unsigned char* VIDEO_MEMORY = (unsigned char*)0xB8000;
const int VGA_WIDTH = 80;
const unsigned char COLOR_CODE = 0x0A; // Bright Green on Black

// Keyboard State and Scancode Definitions
#define LSHIFT_PRESS    0x2A
#define RSHIFT_PRESS    0x36
#define LSHIFT_RELEASE  0xAA
#define RSHIFT_RELEASE  0xB6

// Command Line Interpreter (CLI) Definitions
#define COMMAND_BUFFER_SIZE 256
char command_buffer[COMMAND_BUFFER_SIZE];
int buffer_index = 0;

static int cursor_row = 0;
static int cursor_col = 0;
static int shift_active = 0; 

// --- Function Prototypes (Forward Declarations) ---
// Necessary for functions that call each other out of order (like keyboard_handler -> process_command)
void process_command(); 
void print_u32_hex(u32 n);
void clear_screen();
void print_char(char c);
void print_string(const char* str);
// AI Execution Prototype (NEW)
void execute_aisci_command(AISCI_Command* cmd);
// Simplified US Keyboard Layout Map (Scancode -> ASCII)
unsigned char kbd_us[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   /* 0-15 */
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     /* 16-29 - Enter */
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   /* 30-41 - Left Control */
    0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,      /* 42-53 - Left Shift, Right Shift */
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                 /* 54-69 - Alt, Spacebar, F1..F10 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                      /* 70-85 */
};


// --- Utility Functions ---

// Basic string comparison function (like strcmp)
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Utility function to print a 32-bit unsigned integer in Hexadecimal format
void print_u32_hex(u32 n) {
    char hex_digits[] = "0123456789ABCDEF";
    char result[9]; // 8 hex characters + null terminator
    result[8] = '\0';

    for (int i = 7; i >= 0; i--) {
        result[i] = hex_digits[n & 0xF]; 
        n >>= 4; 
    }
    print_string("0x");
    print_string(result);
}


// --- VGA Output and Cursor Management Functions ---

void clear_screen() {
    for (int k = 0; k < VGA_WIDTH * 25 * 2; k += 2) {
        VIDEO_MEMORY[k] = ' ';
        VIDEO_MEMORY[k + 1] = COLOR_CODE;
    }
    cursor_row = 0;
    cursor_col = 0;
}

void print_char(char c) {
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\b') { 
        if (cursor_col > 0) {
            cursor_col--;
            int offset = (cursor_row * VGA_WIDTH + cursor_col) * 2;
            VIDEO_MEMORY[offset] = ' ';
            VIDEO_MEMORY[offset + 1] = COLOR_CODE;
        }
    } else {
        int offset = (cursor_row * VGA_WIDTH + cursor_col) * 2;
        VIDEO_MEMORY[offset] = c;
        VIDEO_MEMORY[offset + 1] = COLOR_CODE;
        cursor_col++;
        
        if (cursor_col >= VGA_WIDTH) {
            cursor_row++;
            cursor_col = 0;
        }
    }
    // Simple screen scrolling
    if (cursor_row >= 25) {
        clear_screen(); 
    }
}

void print_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}


// --- Keyboard Handler ---

void keyboard_handler() {
    unsigned char scancode = inb(0x60); 
    
    // 1. Handle Shift Key State Changes
    if (scancode == LSHIFT_PRESS || scancode == RSHIFT_PRESS) {
        shift_active = 1;
    } else if (scancode == LSHIFT_RELEASE || scancode == RSHIFT_RELEASE) {
        shift_active = 0;
    } 
    
    // 2. Handle Key Press (Break codes are >= 0x80)
    else if (scancode < 0x80) { 
        unsigned char character = kbd_us[scancode];
        
        if (character == '\n') {
            print_char(character);
            process_command(); 
        } else if (character == '\b') {
            if (buffer_index > 0) {
                buffer_index--;
                print_char(character); 
            }
        } else if (character != 0 && buffer_index < COMMAND_BUFFER_SIZE - 1) {
            // Process character and store it
            if (shift_active) {
                if (character >= 'a' && character <= 'z') {
                    character -= 32; 
                }
            }
            command_buffer[buffer_index++] = character;
            print_char(character);
        }
    }
    
    // Send End of Interrupt (EOI) signal to the Master PIC
    outb(PIC1_CMD, 0x20); 
}


// --- Interrupt and PIC Initialization Functions ---

// The structure for an IDT entry
struct idt_entry {
    unsigned short base_low;    
    unsigned short selector;    
    unsigned char always0;      
    unsigned char flags;        
    unsigned short base_high;   
} __attribute__((packed));

// The structure for the IDT Register (IDTR)
struct idt_ptr {
    unsigned short limit;       
    unsigned int base;          
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idt_ptr_reg;

extern void irq1(); 

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_install() {
    idt_ptr_reg.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_ptr_reg.base = (unsigned int)&idt;

    idt_set_gate(0x21, (unsigned int)irq1, 0x10, 0x8E); 
    
    load_idt(&idt_ptr_reg);
}

void pic_remap() {
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28); 

    outb(PIC1_DATA, 4); 
    outb(PIC2_DATA, 2); 

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, 0xFD); 
    outb(PIC2_DATA, 0xFF); 
}

// --- Paging (Memory Management) Functions ---

typedef unsigned int page_entry;

page_entry page_directory[1024] __attribute__((aligned(4096)));
page_entry page_table[1024] __attribute__((aligned(4096)));

#define PAGE_PRESENT    0x001 
#define PAGE_RW         0x002 

extern void load_cr3(unsigned int);
extern void enable_paging();

void setup_paging() {
    for (int i = 0; i < 1024; i++) {
        page_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_RW;
    }

    page_directory[0] = ((unsigned int)page_table) | PAGE_PRESENT | PAGE_RW;

    for (int i = 1; i < 1024; i++) {
        page_directory[i] = 0;
    }
    
    load_cr3((unsigned int)page_directory);
    enable_paging();
    print_string("Paging Enabled. Memory Protected.\n");
}


// --- AI Core/System State Initialization ---

// src/kernel.c (New function: The core AI execution interface)

// The primary function to execute a command received from the AI Core
void execute_aisci_command(AISCI_Command* cmd) {
    cmd->success_flag = 0; // Assume failure initially
    
    switch (cmd->command_id) {
        case AISCI_CMD_REALLOC_MEM:
            // The AI requested a change to memory allocation (e.g., from OPTIMIZE)
            print_string(" | Executing MEM_REALLOC...\n");
            
            // Placeholder Logic: Assume success if the requested allocation (arg1) is small
            if (cmd->arg1 < 1024) { 
                g_ssb.available_memory_kb -= cmd->arg1; // Simulate memory reduction
                cmd->success_flag = 1;
            }
            break;

        case AISCI_CMD_CHANGE_PRIO:
            // Placeholder Logic: AI changing a process priority
            print_string(" | Executing CHANGE_PRIO...\n");
            // Always succeed for simulation
            cmd->success_flag = 1; 
            break;
            
        case AISCI_CMD_LOAD_MODULE:
            // Placeholder Logic: AI loading a new policy module
            print_string(" | Executing LOAD_MODULE...\n");
            cmd->success_flag = 1;
            break;

        default:
            print_string(" | UNKNOWN AISCI COMMAND.\n");
            cmd->success_flag = 0;
            break;
    }
    
    g_ssb.last_ai_command_timestamp++; // Update command counter
    print_string(cmd->success_flag ? "[AI EXECUTION SUCCESS]\n" : "[AI EXECUTION FAILED]\n");
}


void initialize_ai_structures() {
    g_ssb.total_physical_memory_kb = 65536; 
    g_ssb.available_memory_kb = 60000;
    g_ssb.total_processes = 3;
    g_ssb.ai_status_code = 0;
    g_ssb.last_ai_command_timestamp = 0;
    g_ssb.security_level = 1;
    g_ssb.anomaly_detected = 0;
    g_ssb.current_cpu_load_percent = 5;
    g_ssb.active_threads = 3;
}

// --- Command Line Interpreter (CLI) Logic ---

void process_command() {
    command_buffer[buffer_index] = '\0';

    if (buffer_index > 0) {
        
        // --- 1. Internal Kernel Commands ---
        if (strcmp(command_buffer, "HELP") == 0) {
            print_string("\n[CLI] Available Commands:\n");
            print_string("      HELP - Show this menu\n");
            print_string("      MEM - Display current memory status (SSB)\n");
            print_string("      OPTIMIZE - (AI) Request system optimization\n");
            print_string("      STATUS - (AI) Get AI Core status\n");
            
        } else if (strcmp(command_buffer, "MEM") == 0) {
            print_string("\n[SSB] Memory Status:\n");
            print_string("      Total: ");
            print_u32_hex(g_ssb.total_physical_memory_kb);
            print_string(" KB\n");
            print_string("      Available: ");
            print_u32_hex(g_ssb.available_memory_kb);
            print_string(" KB\n");


        } else if (strcmp(command_buffer, "OPTIMIZE") == 0) {
            print_string("\n[CLI] Routing OPTIMIZE command to AI Core...");
            
            // Set up the AISCI Command Structure
            g_aisci_command.command_id = AISCI_CMD_REALLOC_MEM; // Use Realloc as optimization
            g_aisci_command.target_pid = 1; // Arbitrary Process ID
            g_aisci_command.arg1 = 256;     // AI wants to free up 256 KB
            
            // --- EXECUTE THE AI COMMAND ---
            execute_aisci_command(&g_aisci_command);
            
        } else if (strcmp(command_buffer, "STATUS") == 0) {
            // ... (STATUS block remains the same)    
            
        } else if (strcmp(command_buffer, "STATUS") == 0) {
            print_string("\n[CLI] Querying AI Core Status:\n");
            print_string("      AI Core Status Code: ");
            print_u32_hex(g_ssb.ai_status_code);
            print_string("\n");
            print_string("      Security Level: ");
            print_u32_hex(g_ssb.security_level);
            print_string("\n");
            
        } else {
            print_string("\n[CLI] Unknown command. Type HELP.\n");
        }
    }
    
    // Reset the buffer and prompt
    buffer_index = 0;
    print_char('>');
}


// --- Kernel Main Entry Point ---

void kernel_main() {
    clear_screen();
    print_string("SingularityOS booting...\n");
    print_string("AI Core: Offline (Phase 1 Complete)\n");
    print_string("--- Initializing Hardware and Memory ---\n");

    pic_remap();
    idt_install();
    setup_paging(); 
    initialize_ai_structures(); 
    
    // Enable interrupts in the CPU
    asm volatile("sti"); 
    
    print_string("--- System Ready (SingularityOS Command Line)\n\n");
    print_char('>'); 

    while (1) {}
}
