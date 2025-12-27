#ifndef SINGULARITY_DEFS_H
#define SINGULARITY_DEFS_H

// --- Standard Types ---
typedef unsigned int u32;

// --- AI System Call Interface (AISCI) Definitions ---

// Command IDs for the AI Core to execute in the kernel
#define AISCI_CMD_REALLOC_MEM   1
#define AISCI_CMD_CHANGE_PRIO   2
#define AISCI_CMD_LOAD_MODULE   3

// Structure used for communication from AI Core to Kernel
typedef struct {
    u32 command_id;         // The requested action (e.g., REALLOC_MEM)
    u32 target_pid;         // The process ID the action targets
    u32 arg1;               // Argument 1 (e.g., amount of memory to reallocate, new priority)
    u32 success_flag;       // Set by the kernel after execution (1=Success, 0=Failure)
} AISCI_Command;

// Structure holding critical system state (the AI's input/monitoring data)
typedef struct {
    u32 total_physical_memory_kb;
    u32 available_memory_kb;
    u32 total_processes;
    u32 active_threads;
    u32 current_cpu_load_percent;
    u32 ai_status_code;
    u32 security_level;
    u32 anomaly_detected;
    u32 last_ai_command_timestamp;
} SystemStateBlock;


// --- Task Context Structure (Scheduler) ---
// Holds the CPU registers (the context) for saving/restoring a task
typedef struct {
    u32 gs, fs, es, ds;                                  // Data segments
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;          // General purpose registers
    u32 eip, cs, eflags, useresp, ss;                    // State pushed by the CPU (by interrupt hardware)
} __attribute__((packed)) TaskContext;


// src/singularity_defs.h (TaskControlBlock Fix)

typedef struct {
    u32 pid;        // <-- MISSING FIELD (Error 1, 4)
    u32 state;      // <-- MISSING FIELD (Error 2, 5)
    u32 priority;   // <-- MISSING FIELD (Error 3, 6)
    
    // This is the field we fixed in the previous step: the address of the saved TaskContext
    u32 context;    // <-- CRITICAL: Must be u32 to store the memory address
    
    u32 stack_base;
    // You may have other members here, but these are the minimum needed.
} __attribute__((packed)) TaskControlBlock;

#endif // SINGULARITY_DEFS_H
