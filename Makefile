# ==========================
#  KuvixOS-V2 Makefile
# ==========================

CC = gcc
LD = gcc

BUILD  = build
ISO    = iso
KERNEL = $(BUILD)/kernel.elf
IMAGE  = KuvixOS.iso

CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra \
          -fno-pie -fno-stack-protector \
          -nostdlib -nostartfiles \
          -Iinclude

ASFLAGS = -m32
LDFLAGS = -m32 -T linker.ld -nostdlib -ffreestanding -fno-pie

SRC_S = \
    boot/boot.S

SRC_C = \
    kernel/kmain.c \
    kernel/printk.c \
    kernel/panic.c \
    kernel/vga.c \
    kernel/serial.c \

OBJS = \
    $(SRC_S:%.S=$(BUILD)/%.o) \
    $(SRC_C:%.c=$(BUILD)/%.o)

# --------------------------
# Varsayılan hedef
# --------------------------

all: $(KERNEL)

# --------------------------
# Build
# --------------------------

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

# --------------------------
# ISO
# --------------------------

iso: $(KERNEL)
	rm -rf $(ISO)
	mkdir -p $(ISO)/boot/grub

	cp $(KERNEL) $(ISO)/boot/kernel.elf

	echo 'set timeout=0' >  $(ISO)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO)/boot/grub/grub.cfg
	echo '' >> $(ISO)/boot/grub/grub.cfg
	echo 'menuentry "KuvixOS V2" {' >> $(ISO)/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.elf' >> $(ISO)/boot/grub/grub.cfg
	echo '  boot' >> $(ISO)/boot/grub/grub.cfg
	echo '}' >> $(ISO)/boot/grub/grub.cfg

	grub2-mkrescue -o $(IMAGE) $(ISO) > /dev/null 2>&1

# --------------------------
# RUN
# --------------------------

run: iso
	qemu-system-i386 \
		-cdrom $(IMAGE) \
		-m 256M \
		-serial stdio \
		-no-reboot

# --------------------------
# CLEAN
# --------------------------

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)
