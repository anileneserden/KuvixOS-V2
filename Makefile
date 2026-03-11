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
#          -DKBD_SERIAL_DEBUG

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
    kernel/drivers/rtc/rtc.c \
    kernel/drivers/video/fb_console.c \
    kernel/drivers/video/fb.c \
    kernel/drivers/video/gfx.c \
    kernel/drivers/ata_pio.c \
    kernel/drivers/power.c \
    kernel/drivers/ps2.c \
    kernel/drivers/vga_font.c \
    kernel/drivers/virtio_blk.c \
    kernel/exec/kef/kef_api.c \
    kernel/exec/kef/kef_loader.c \
    kernel/exec/kef/kef_runtime.c \
    kernel/exec/kef/kef_seed.c \
    kernel/fs/fs_init.c \
    kernel/fs/kvxfs.c \
    kernel/fs/ramfs.c \
    kernel/fs/toyfs_image.c \
    kernel/fs/toyfs.c \
    kernel/fs/vfs.c \
    kernel/memory/kmalloc.c \
    kernel/system/removable.c \
    ui/apps/calculator.c \
    ui/apps/demo.c \
    ui/apps/designer.c \
    ui/apps/demo_font.c \
    ui/apps/file_manager.c \
    ui/apps/grid_demo.c \
    ui/apps/kef_host.c \
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
    ui/controls/ui_context.c \
    ui/dialogs/open_dialog.c \
    ui/dialogs/save_dialog.c \
    ui/dialogs/messagebox.c \
    ui/font/font8x8_basic.c \
    ui/font/font8x16_basic.c \
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
    lib/string/string.c \

COMMAND_SOURCES = $(wildcard kernel/commands/*.c)
SRC_C += $(COMMAND_SOURCES)

# ==========================
# KEF Apps (build + embed)
# ==========================

KEF_HELLO_DIR := apps_kef/hello
KEF_HELLO_ELF := $(KEF_HELLO_DIR)/hello.elf
KEF_HELLO_BIN := $(KEF_HELLO_DIR)/hello.bin
KEF_HELLO_KEF := $(KEF_HELLO_DIR)/hello.kef

KEF_BLOB_OBJ  := $(BUILD)/kernel/exec/kef/hello_kef_blob.o

KEF_APP_CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra \
	-fno-pie -fno-pic -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-nostdlib -nostartfiles \
	-Iinclude

KEF_APP_LDFLAGS = -m32 -nostdlib -ffreestanding -fno-pie \
	-Wl,-T,$(KEF_HELLO_DIR)/link.ld \
	-Wl,--emit-relocs \
	-Wl,--build-id=none

$(KEF_HELLO_ELF): $(KEF_HELLO_DIR)/main.c $(KEF_HELLO_DIR)/main.designer.c $(KEF_HELLO_DIR)/kefstring.c $(KEF_HELLO_DIR)/link.ld
	@mkdir -p $(KEF_HELLO_DIR)
	$(CC) $(KEF_APP_CFLAGS) -c $(KEF_HELLO_DIR)/main.c -o $(KEF_HELLO_DIR)/main.o
	$(CC) $(KEF_APP_CFLAGS) -c $(KEF_HELLO_DIR)/main.designer.c -o $(KEF_HELLO_DIR)/main.designer.o
	$(CC) $(KEF_APP_CFLAGS) -c $(KEF_HELLO_DIR)/kefstring.c -o $(KEF_HELLO_DIR)/kef_string.o
	$(LD) $(KEF_APP_LDFLAGS) -o $(KEF_HELLO_ELF) \
		$(KEF_HELLO_DIR)/main.o $(KEF_HELLO_DIR)/main.designer.o $(KEF_HELLO_DIR)/kef_string.o

$(KEF_HELLO_BIN): $(KEF_HELLO_ELF)
	objcopy -O binary $(KEF_HELLO_ELF) $(KEF_HELLO_BIN)

$(KEF_HELLO_KEF): FORCE $(KEF_HELLO_BIN) $(KEF_HELLO_ELF) tools/mk_kef.py
	@mkdir -p $(dir $(KEF_HELLO_KEF))
	@ENTRY=$$(nm -n $(KEF_HELLO_ELF) | awk '/ _start$$/ {print "0x"$$1; exit}'); \
	if [ -z "$$ENTRY" ]; then echo "[KEF] ERROR: _start not found in $(KEF_HELLO_ELF)"; exit 1; fi; \
	echo "[KEF] hello entry = $$ENTRY"; \
	python3 tools/mk_kef.py $(KEF_HELLO_ELF) $(KEF_HELLO_BIN) $(KEF_HELLO_KEF) $$ENTRY 0

$(KEF_BLOB_OBJ): $(KEF_HELLO_KEF)
	@mkdir -p $(dir $@)
	objcopy -I binary -O elf32-i386 -B i386 $(KEF_HELLO_KEF) $@

OBJS = $(SRC_S:%.S=$(BUILD)/%.o) \
       $(SRC_ASM:%.asm=$(BUILD)/%.o) \
       $(SRC_C:%.c=$(BUILD)/%.o) \
       $(KEF_BLOB_OBJ)

# --- Kurallar ---

all: $(KEF_BLOB_OBJ) $(KERNEL)

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
		-m 256M -serial stdio

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)

.PHONY: FORCE clean run iso all