// --- Includes ---
#include "idt.h" 
#include "singularity_defs.h" 


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// --- Assembly Helpers Prototypes
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void load_cr3(unsigned int);
extern void enable_paging();
extern void load_idt(void*); 

// --- Global Constants and Definitions ---
SystemStateBlock g_ssb;
AISCI_Command g_aisci_command;

// PIC I/O Port Definitions 
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

// RTC I/O Ports
#define CMOS_ADDRESS 0x70 // RTC address port
#define CMOS_DATA    0x71 // RTC data port

// RTC Registers
#define RTC_SECONDS      0x00
#define RTC_MINUTES      0x02
#define RTC_HOURS        0x04

// PIT (Timer) Definitions
#define PIT_CMD_PORT    0x43
#define PIT_DATA_PORT   0x40
#define TICKS_PER_SECOND 18 // Approx. ticks for 18.2 Hz rate

// Clock Variables (RTC-synced)
u32 timer_ticks = 0; 
u32 seconds = 0;
u32 minutes = 0;
u32 hours = 0;

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

// --- Function Prototypes ---
void process_command(); 
void print_u32_hex(u32 n);
void clear_screen();
void print_char(char c);
void print_string(const char* str);
void execute_aisci_command(AISCI_Command* cmd);

// Timer and Interrupt Prototypes
void timer_install();
void schedule_and_eoi(); 
void update_clock();
void print_clock_ui();

// RTC Clock Functions
void read_rtc_time();
u8 get_rtc_register(int reg);

extern void irq0(); // Timer IRQ 0 Assembly Stub
extern void irq1(); // Keyboard IRQ 1 Assembly Stub


// Simplified US Keyboard Layout Map 
unsigned char kbd_us[128] =
{ 
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   
    0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,      
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                      
};


// --- Utility Functions ---
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void print_u32_hex(u32 n) { 
    char hex_digits[] = "0123456789ABCDEF";
    char result[9]; 
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
    if (cursor_row >= 25) {
        clear_screen(); 
    }
}

void print_string(const char* str) { 
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}


// --- Clock UI and Logic Functions --- 

void print_clock_ui() {
    int old_row = cursor_row;
    int old_col = cursor_col;
    
    // Fixed location: Row 0, Col 70
    int offset = (0 * VGA_WIDTH + 70) * 2;
    
    // Ensure hours are 24-hour format for display
    u32 display_hours = hours % 24; 
    
    #define PRINT_DIGIT(val) \
        VIDEO_MEMORY[offset++] = '0' + ((val) / 10); \
        VIDEO_MEMORY[offset++] = COLOR_CODE; \
        VIDEO_MEMORY[offset++] = '0' + ((val) % 10); \
        VIDEO_MEMORY[offset++] = COLOR_CODE;
        
    // HH
    PRINT_DIGIT(display_hours);
    
    // Separator
    VIDEO_MEMORY[offset++] = ':';
    VIDEO_MEMORY[offset++] = COLOR_CODE;
    
    // MM
    PRINT_DIGIT(minutes);
    
    // Separator
    VIDEO_MEMORY[offset++] = ':';
    VIDEO_MEMORY[offset++] = COLOR_CODE;
    
    // SS
    PRINT_DIGIT(seconds);
    
    #undef PRINT_DIGIT

    cursor_row = old_row;
    cursor_col = old_col;
}

void update_clock() {
    // Only update every (TICKS_PER_SECOND) ticks, which is about once per second
    if ((timer_ticks % TICKS_PER_SECOND) == 0) {
        seconds++;
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
            if (minutes >= 60) {
                minutes = 0;
                hours++;
                if (hours >= 24) {
                    // Reset hours at 24.
                    hours = 0;
                }
            }
        }
        print_clock_ui(); 
    }
}


// --- RTC Reading Functions ---

// Helper function to read a specific RTC register
u8 get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// Function to read the time from the RTC and initialize variables
void read_rtc_time() {
    u8 status_b = get_rtc_register(0x0B); 
    
    // Read raw BCD values from RTC
    u8 raw_hours = get_rtc_register(RTC_HOURS);
    u8 raw_minutes = get_rtc_register(RTC_MINUTES);
    u8 raw_seconds = get_rtc_register(RTC_SECONDS);

    // BCD to Binary Conversion 
    if (!(status_b & 0x04)) {
        raw_seconds = (raw_seconds & 0x0F) + ((raw_seconds / 16) * 10);
        raw_minutes = (raw_minutes & 0x0F) + ((raw_minutes / 16) * 10);
        raw_hours = (raw_hours & 0x0F) + ((raw_hours / 16) * 10);
    }
    
    // Initialize global clock variables with RTC time 
    hours = raw_hours;
    minutes = raw_minutes;
    seconds = raw_seconds;
    
    // --- TIME ZONE CORRECTION: Convert UTC to IST ---
    // IST = UTC + 5 hours 30 minutes
    
    // Add 5 hours
    hours = (hours + 5) % 24;
    
    // Add 30 minutes, handling the hour overflow
    minutes += 30;
    if (minutes >= 60) {
        minutes -= 60;
        hours = (hours + 1) % 24;
    }
}


// --- Timer and Scheduler ---

void timer_install() {
    // Divisor = 1193180 / 18.222 = 65536 (0xFFFF)
    u32 divisor = 65536; 
    
    // Command Byte
    outb(PIT_CMD_PORT, 0x36); 
    
    // Send the divisor 
    outb(PIT_DATA_PORT, (unsigned char)(divisor & 0xFF));
    outb(PIT_DATA_PORT, (unsigned char)((divisor >> 8) & 0xFF));
}

void schedule_and_eoi() {
    timer_ticks++;
    
    update_clock(); 
    
    // Send EOI to the Master PIC
    outb(PIC1_CMD, 0x20); 
}


// --- Keyboard Handler ---
void keyboard_handler() { 
    unsigned char scancode = inb(0x60); 
    
    if (scancode == LSHIFT_PRESS || scancode == RSHIFT_PRESS) {
        shift_active = 1;
    } else if (scancode == LSHIFT_RELEASE || scancode == RSHIFT_RELEASE) {
        shift_active = 0;
    } 
    
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
            if (shift_active) {
                if (character >= 'a' && character <= 'z') {
                    character -= 32; 
                }
            }
            command_buffer[buffer_index++] = character;
            print_char(character);
        }
    }
    
    outb(PIC1_CMD, 0x20); 
}


// --- Interrupt and PIC Initialization Functions ---

struct idt_entry { 
    unsigned short base_low;    
    unsigned short selector;    
    unsigned char always0;      
    unsigned char flags;        
    unsigned short base_high;   
} __attribute__((packed));

struct idt_ptr { 
    unsigned short limit;       
    unsigned int base;          
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idt_ptr_reg;


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

    // Timer Interrupt 
    idt_set_gate(0x20, (unsigned int)irq0, 0x10, 0x8E); 
    
    // Keyboard Interrupt
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

    // Unmask IRQ 0 (Timer) and IRQ 1 (Keyboard)
    outb(PIC1_DATA, 0xFC); 
    outb(PIC2_DATA, 0xFF); 
}

// --- Paging Functions ---
typedef unsigned int page_entry;

page_entry page_directory[1024] __attribute__((aligned(4096)));
page_entry page_table[1024] __attribute__((aligned(4096)));

#define PAGE_PRESENT    0x001 
#define PAGE_RW         0x002 

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


// --- AI Core/System State Initialization and CLI Logic ---

void execute_aisci_command(AISCI_Command* cmd) { 
    cmd->success_flag = 0; 
    
    switch (cmd->command_id) {
        case AISCI_CMD_REALLOC_MEM:
            print_string(" | Executing MEM_REALLOC...\n");
            if (cmd->arg1 < 1024) { 
                g_ssb.available_memory_kb -= cmd->arg1; 
                cmd->success_flag = 1;
            }
            break;

        case AISCI_CMD_CHANGE_PRIO:
            print_string(" | Executing CHANGE_PRIO...\n");
            cmd->success_flag = 1; 
            break;
            
        case AISCI_CMD_LOAD_MODULE:
            print_string(" | Executing LOAD_MODULE...\n");
            cmd->success_flag = 1;
            break;

        default:
            print_string(" | UNKNOWN AISCI COMMAND.\n");
            cmd->success_flag = 0;
            break;
    }
    
    g_ssb.last_ai_command_timestamp++;
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

void process_command() { 
    command_buffer[buffer_index] = '\0';

    if (buffer_index > 0) {
        
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
            
            g_aisci_command.command_id = AISCI_CMD_REALLOC_MEM;
            g_aisci_command.target_pid = 1; 
            g_aisci_command.arg1 = 256;     
            
            execute_aisci_command(&g_aisci_command);
            
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
    
    buffer_index = 0;
    print_char('>');
}


// --- Kernel Main Entry Point ---

void kernel_main() {
    clear_screen();
    print_string("SingularityOS booting...\n");
    print_string("AI Core: Offline (Phase 1 Complete)\n");
    print_string("--- Initializing Hardware and Memory ---\n");

    // All hardware and IDT setup must happen before interrupts are enabled

    pic_remap();
    timer_install(); 
    idt_install();
    setup_paging(); 
    initialize_ai_structures(); 

    // CRITICAL
    read_rtc_time(); 
    
    // CRITICAL
    print_clock_ui(); 
    
    // Enable interrupts 
    asm volatile("sti"); 
    
    print_string("--- System Ready (SingularityOS Command Line)\n\n");
    print_char('>'); 

    while (1) {}
}
