# ==========================
#  KuvixOS-V2 Makefile
# ==========================

CC = gcc
LD = gcc
AS = nasm

BUILD  = build
ISO    = iso
KERNEL = $(BUILD)/kernel.elf
IMAGE  = KuvixOS.iso

CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra \
          -fno-pie -fno-stack-protector -fno-builtin \
          -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
          -nostdlib -nostartfiles \
          -Iinclude -DTIMEZONE_OFFSET=3

ASFLAGS   = -m32
NASMFLAGS = -f elf32

LDFLAGS = -m32 -T linker.ld -nostdlib -ffreestanding -fno-pie \
          -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments \
          -Wl,--no-gc-sections

LIBGCC := $(shell $(CC) $(CFLAGS) -m32 -print-libgcc-file-name)

# --- Kaynak Dosyalar ---
SRC_S = boot/boot.S
SRC_ASM = kernel/arch/x86/interrupt_entry.asm

SRC_C = \
    kernel/kmain.c \
    kernel/panic.c \
    kernel/printk.c \
    kernel/serial.c \
    kernel/time.c \
    kernel/user.c \
    kernel/vga.c \
    kernel/arch/x86/gdt.c \
    kernel/arch/x86/idt.c \
    kernel/block/block.c \
    kernel/block/blockdev.c \
    kernel/debug/debug_kbd.c \
    kernel/drivers/input/keymaps/layout.c \
    kernel/drivers/input/keymaps/us.c \
    kernel/drivers/input/keymaps/trq.c \
    kernel/drivers/input/keyboard.c \
    kernel/drivers/input/mouse_ps2.c \
    kernel/drivers/net/e1000.c \
    kernel/drivers/net/net.c \
    kernel/drivers/net/pci.c \
    kernel/drivers/rtc/rtc.c \
    kernel/drivers/video/fb_console.c \
    kernel/drivers/video/fb.c \
    kernel/drivers/video/gfx.c \
    kernel/drivers/video/font/font8x8_basic.c \
    kernel/drivers/video/font/font8x16_basic.c \
    kernel/drivers/ata_pio.c \
    kernel/drivers/power.c \
    kernel/drivers/vga_font.c \
    kernel/drivers/virtio_blk.c \
    kernel/fs/fs_init.c \
    kernel/fs/kvxfs.c \
    kernel/fs/ramfs.c \
    kernel/fs/toyfs_image.c \
    kernel/fs/toyfs.c \
    kernel/fs/vfs.c \
    kernel/memory/kmalloc.c \
    kernel/system/removable.c \
    init/init.c \
    init/session.c \
    lib/commands/commands.c \
    lib/service/service_registry.c \
    lib/service/service.c \
    lib/shell/shell.c \
    lib/string/string.c

COMMAND_SOURCES = $(wildcard kernel/commands/*.c)
SRC_C += $(COMMAND_SOURCES)

OBJS = $(SRC_S:%.S=$(BUILD)/%.o) \
       $(SRC_ASM:%.asm=$(BUILD)/%.o) \
       $(SRC_C:%.c=$(BUILD)/%.o)

# --- Varsayılan hedef ---
all: iso

# --- Derleme kuralları ---
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(NASMFLAGS) $< -o $@

$(KERNEL): $(OBJS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC)

# --- ISO üret ---
iso: $(KERNEL)
	rm -rf $(ISO)
	mkdir -p $(ISO)/boot/grub
	cp $(KERNEL) $(ISO)/boot/kernel.elf
	@echo 'set timeout=2' >  $(ISO)/boot/grub/grub.cfg
	@echo 'set default=0' >> $(ISO)/boot/grub/grub.cfg
	@echo 'insmod vbe' >> $(ISO)/boot/grub/grub.cfg
	@echo 'insmod vga' >> $(ISO)/boot/grub/grub.cfg
	@echo 'insmod video_bochs' >> $(ISO)/boot/grub/grub.cfg
	@echo 'insmod video_cirrus' >> $(ISO)/boot/grub/grub.cfg
	@echo '' >> $(ISO)/boot/grub/grub.cfg
	@echo 'menuentry "KuvixOS V2" {' >> $(ISO)/boot/grub/grub.cfg
	@echo '  set gfxmode=1024x768x32' >> $(ISO)/boot/grub/grub.cfg
	@echo '  set gfxpayload=keep' >> $(ISO)/boot/grub/grub.cfg
	@echo '  multiboot /boot/kernel.elf' >> $(ISO)/boot/grub/grub.cfg
	@echo '  boot' >> $(ISO)/boot/grub/grub.cfg
	@echo '}' >> $(ISO)/boot/grub/grub.cfg
	grub2-mkrescue -o $(IMAGE) $(ISO)

# --- Çalıştır ---
run: iso
	@if mountpoint -q /home/anil/KuvixFSMountSystem/mnt; then \
		echo "⚠️  UYARI: disk.img şu anda FUSE (mnt) üzerinde bağlı! Güvenlik için unmount ediliyor..."; \
		fusermount -u /home/anil/KuvixFSMountSystem/mnt || true; \
	fi

	@chmod 666 /home/anil/KuvixOS-V2/main/disk.img

	qemu-system-i386 -cdrom $(IMAGE) \
		-drive file=/home/anil/KuvixOS-V2/main/disk.img,format=raw,index=0,media=disk \
		-device e1000,netdev=n0 -netdev user,id=n0 \
		-m 256M -serial stdio

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)

.PHONY: all iso run clean
