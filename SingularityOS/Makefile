# Makefile for SingularityOS - Phase 1 (Keyboard Input Ready)

ARCH := i386
CC := gcc
LD := ld
AS := nasm

# Build directories
BUILD_DIR := build
SRC_DIR := src
ISO_DIR := isodir

# Source Files
BOOT_ASM := $(SRC_DIR)/boot.asm
KERNEL_C := $(SRC_DIR)/kernel.c
ISR_ASM := $(SRC_DIR)/isr.asm
LINKER_SCRIPT := $(SRC_DIR)/linker.ld

# Object files (The intermediate compiled files)
BOOT_OBJ := $(BUILD_DIR)/boot.o
KERNEL_OBJ := $(BUILD_DIR)/kernel.o
ISR_OBJ := $(BUILD_DIR)/isr.o
OBJECTS := $(BOOT_OBJ) $(KERNEL_OBJ) $(ISR_OBJ)

# Output files
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
ISO_FILE := singularityos.iso

# Compiler flags
CFLAGS := -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -c
ASFLAGS := -f elf32

.PHONY: all clean run iso

all: $(KERNEL_BIN)

# Rule to create the build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --- Compilation Rules ---

# Rule to assemble the Assembly boot file
$(BOOT_OBJ): $(BOOT_ASM) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Rule to assemble the Assembly ISR file
$(ISR_OBJ): $(ISR_ASM) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Rule to compile the C kernel file
$(KERNEL_OBJ): $(KERNEL_C) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# --- Linking Rule ---

# Rule to link the object files into the final kernel binary
# The linker script is specified using -T, and only object files ($OBJECTS) are passed as input.
$(KERNEL_BIN): $(OBJECTS) $(LINKER_SCRIPT)
	$(LD) -m elf_i386 -T $(LINKER_SCRIPT) -o $@ $(OBJECTS)

# --- Disk Image Creation (ISO) ---

# Create the ISO directory structure and config file for GRUB
$(ISO_DIR): $(KERNEL_BIN)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "SingularityOS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg

# Create the final bootable ISO image
$(ISO_FILE): $(ISO_DIR)
	grub-mkrescue -o $@ $(ISO_DIR)

# --- Primary Commands ---

# Clean up all generated files
clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_FILE)

# Build the ISO and run in QEMU
run: $(ISO_FILE)
	qemu-system-i386 -cdrom $(ISO_FILE)
