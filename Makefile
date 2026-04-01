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
          -fno-pie -fno-stack-protector \
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
    kernel/drivers/ata_pio.c \
    kernel/drivers/power.c \
    kernel/drivers/ps2.c \
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
    kernel/system/seed_files.c \
    ui/apps/calculator.c \
    ui/apps/controls_test.c \
    ui/apps/demo.c \
    ui/apps/designer.c \
    ui/apps/demo_font.c \
    ui/apps/file_manager.c \
    ui/apps/grid_demo.c \
    ui/apps/kbi_viewer.c \
    ui/apps/kuvix_browser.c \
    ui/apps/kuvix_store.c \
    ui/apps/memmon.c \
    ui/apps/notepad.c \
    ui/apps/pixel_draw_app.c \
    ui/apps/run.c \
    ui/apps/scroll_demo.c \
    ui/apps/settings.c \
    ui/apps/setup_wizard.c \
    ui/apps/terminal.c \
    ui/bitmaps/icons/icon_close_16.c \
    ui/bitmaps/icons/icon_max_16.c \
    ui/bitmaps/icons/icon_min_16.c \
    ui/controls/button2.c \
    ui/controls/combobox2.c \
    ui/controls/control.c \
    ui/controls/label2.c \
    ui/controls/panel2.c \
    ui/controls/textbox2.c \
    ui/controls/ui_context.c \
    ui/dialogs/open_dialog.c \
    ui/dialogs/save_dialog.c \
    ui/dialogs/messagebox.c \
    ui/font/font8x8_basic.c \
    ui/font/font8x16_basic.c \
    ui/html/html_dom.c \
    ui/html/html_parser.c \
    ui/html/html_render.c \
    ui/html/html_tokenizer.c \
    ui/html/url_resolver.c \
    ui/wm/hittest.c \
    ui/app_manager.c \
    ui/context_menu.c \
    ui/cursor.c \
    ui/debug_overlay.c \
    ui/desktop_icons.c \
    ui/desktop_seed.c \
    ui/desktop.c \
    ui/icons/ui_icons.c \
    ui/inputtest.c \
    ui/mouse.c \
    ui/net_status.c \
    ui/notification.c \
    ui/power_screen.c \
    ui/select.c \
    ui/session.c \
    ui/theme_bootstrap.c \
    ui/theme_builtin_data.c \
    ui/theme_parser.c \
    ui/theme_runtime.c \
    ui/theme_session.c \
    ui/topbar.c \
    ui/ui_button.c \
    ui/ui_init.c \
    ui/ui_label.c \
    ui/ui_settings.c \
    ui/wallpaper.c \
    ui/window_chrome.c \
    ui/window.c \
    ui/widgets/textbox.c \
    ui/wm.c \
    ui/theme_builtin.c \
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
	@test -f disk.img || dd if=/dev/zero of=disk.img bs=1M count=128
	@chmod 666 disk.img
	qemu-system-i386 -cdrom $(IMAGE) \
		-drive file=disk.img,format=raw,index=0,media=disk \
		-device e1000,netdev=n0 -netdev user,id=n0 \
		-m 256M -serial stdio

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)

.PHONY: all iso run clean