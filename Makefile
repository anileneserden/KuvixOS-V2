# ==========================
#  KuvixOS-V2 Makefile
# ==========================

CC = gcc
LD = gcc
AS = nasm
# 64-bit matematik işlemleri için gerekli yardımcı kütüphane
LIBGCC := $(shell $(CC) $(CFLAGS) -m32 -print-libgcc-file-name)

BUILD  = build
ISO    = iso
KERNEL = $(BUILD)/kernel.elf
IMAGE  = KuvixOS.iso

CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra \
          -fno-pie -fno-stack-protector \
          -nostdlib -nostartfiles \
          -Iinclude -DTIMEZONE_OFFSET=3

ASFLAGS = -m32
NASMFLAGS = -f elf32

LDFLAGS = -m32 -T linker.ld -nostdlib -ffreestanding -fno-pie \
          -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments \
          -Wl,--no-gc-sections

# --- Kaynak Dosyalar ---
SRC_S = boot/boot.S
SRC_ASM = kernel/arch/x86/interrupt_entry.asm
SRC_C = \
    kernel/kmain.c \
    kernel/printk.c \
    kernel/panic.c \
    kernel/vga.c \
    kernel/serial.c \
    kernel/time.c \
    kernel/memory/kmalloc.c \
    kernel/block/block.c \
    kernel/block/blockdev.c \
    kernel/drivers/video/fb.c \
    kernel/drivers/video/gfx.c \
    kernel/drivers/ata_pio.c \
    kernel/drivers/virtio_blk.c \
    kernel/drivers/ps2.c \
    kernel/drivers/vga_font.c \
    kernel/drivers/input/keyboard.c \
    kernel/drivers/input/mouse_ps2.c \
    kernel/drivers/rtc/rtc.c \
    kernel/drivers/power.c \
    kernel/drivers/input/keymaps/layout.c \
    kernel/drivers/input/keymaps/us.c \
    kernel/drivers/input/keymaps/trq.c \
    kernel/fs/vfs.c \
    kernel/fs/ramfs.c \
    kernel/fs/kvxfs.c \
    kernel/fs/toyfs.c \
    kernel/fs/toyfs_image.c \
    kernel/fs/fs_init.c \
    kernel/ui/apps/file_manager.c \
    kernel/ui/apps/notepad.c \
    kernel/ui/apps/settings.c \
    kernel/ui/apps/terminal.c \
    kernel/ui/bitmaps/icons/icon_close_16.c \
    kernel/ui/bitmaps/icons/icon_max_16.c \
    kernel/ui/bitmaps/icons/icon_min_16.c \
    kernel/ui/cursor.c \
    kernel/ui/desktop_icons.c \
    kernel/ui/desktop.c \
    kernel/ui/installer.c \
    kernel/ui/dialogs/save_dialog.c \
    kernel/ui/power_screen.c \
    kernel/ui/select.c \
    kernel/ui/wm/hittest.c \
    kernel/ui/wm.c \
    kernel/ui/messagebox.c \
    kernel/ui/mouse.c \
    kernel/ui/notification.c \
    kernel/ui/wallpaper.c \
    kernel/ui/window.c \
    kernel/ui/window_chrome.c \
    kernel/ui/app_manager.c \
    kernel/ui/context_menu.c \
    kernel/ui/theme_builtin.c \
    kernel/ui/theme_runtime.c \
    kernel/ui/theme_bootstrap.c \
    kernel/ui/theme_parser.c \
    kernel/ui/theme_builtin_data.c \
    kernel/ui/topbar.c \
    kernel/ui/ui_button.c \
    lib/commands/commands.c \
    lib/service/service.c \
    lib/service/service_registry.c \
    lib/shell/shell.c \
    lib/string/string.c \
    lib/ui/font/font8x8_basic.c \
    lib/math.c \
    kernel/arch/x86/gdt.c \
    kernel/arch/x86/idt.c

COMMAND_SOURCES = $(wildcard kernel/commands/*.c)
SRC_C += $(COMMAND_SOURCES)

OBJS = $(SRC_S:%.S=$(BUILD)/%.o) \
       $(SRC_ASM:%.asm=$(BUILD)/%.o) \
       $(SRC_C:%.c=$(BUILD)/%.o)

# --- Kurallar ---

all: $(KERNEL)

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

run: iso
	@test -f disk.img || dd if=/dev/zero of=disk.img bs=1M count=10
	@test -f disk2.img || dd if=/dev/zero of=disk2.img bs=1M count=5
	@chmod 666 disk.img disk2.img
	qemu-system-i386 -cdrom $(IMAGE) \
		-drive file=disk.img,format=raw,index=0,media=disk \
		-drive file=disk2.img,format=raw,index=1,media=disk \
		-m 256M -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)