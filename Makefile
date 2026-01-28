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
LDFLAGS = -m32 -T linker.ld -nostdlib -ffreestanding -fno-pie \
          -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments

# --- Kaynak Dosyalar ---

SRC_S = boot/boot.S

# 1. Sabit çekirdek dosyaları
SRC_C = \
    kernel/kmain.c \
    kernel/printk.c \
    kernel/panic.c \
    kernel/vga.c \
    kernel/serial.c \
    kernel/drivers/kbd.c \
    kernel/drivers/ps2.c \
    kernel/layout/layout.c \
    kernel/layout/trq.c \
    lib/shell/shell.c \
    lib/commands/commands.c \
    lib/service/service.c \
    lib/service/service_registry.c \
    lib/string/string.c

# 2. OTOMATİK KOMUT TARAMA: kernel/commands/ altındaki tüm .c dosyalarını bulur
COMMAND_SOURCES = $(wildcard kernel/commands/*.c)
SRC_C += $(COMMAND_SOURCES)

# Nesne dosyaları listesi (Source dosyalarının build içindeki karşılıkları)
OBJS = \
    $(SRC_S:%.S=$(BUILD)/%.o) \
    $(SRC_C:%.c=$(BUILD)/%.o)

# --- Kurallar ---

all: $(KERNEL)

# Nesne dosyalarını derleme (Klasörleri otomatik oluşturur)
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# Linkleme
$(KERNEL): $(OBJS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lgcc

# ISO Oluşturma
iso: $(KERNEL)
	rm -rf $(ISO)
	mkdir -p $(ISO)/boot/grub
	cp $(KERNEL) $(ISO)/boot/kernel.elf
	@echo 'set timeout=0' >  $(ISO)/boot/grub/grub.cfg
	@echo 'set default=0' >> $(ISO)/boot/grub/grub.cfg
	@echo '' >> $(ISO)/boot/grub/grub.cfg
	@echo 'menuentry "KuvixOS V2" {' >> $(ISO)/boot/grub/grub.cfg
	@echo '  multiboot /boot/kernel.elf' >> $(ISO)/boot/grub/grub.cfg
	@echo '  boot' >> $(ISO)/boot/grub/grub.cfg
	@echo '}' >> $(ISO)/boot/grub/grub.cfg
	grub2-mkrescue -o $(IMAGE) $(ISO) > /dev/null 2>&1

run: iso
	qemu-system-i386 -cdrom $(IMAGE) -m 256M -serial stdio -no-reboot

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)