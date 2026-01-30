# ==========================
#  KuvixOS-V2 Makefile
# ==========================

CC = gcc
LD = gcc

BUILD  = build
ISO    = iso
KERNEL = $(BUILD)/kernel.elf
IMAGE  = KuvixOS.iso

# -Iinclude sayesinde VS Code ve Derleyici dosyaları include/ altında bulur
CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra \
          -fno-pie -fno-stack-protector \
          -nostdlib -nostartfiles \
          -Iinclude

ASFLAGS = -m32
# Makefile içindeki LDFLAGS satırını şu şekilde güncelle:
LDFLAGS = -m32 -T linker.ld -nostdlib -ffreestanding -fno-pie \
          -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments \
          -Wl,--no-gc-sections

# --- Kaynak Dosyalar ---

SRC_S = boot/boot.S

# Yeni klasör yapına göre güncellenmiş SRC_C listesi
SRC_C = \
    kernel/kmain.c \
    kernel/printk.c \
    kernel/panic.c \
    kernel/vga.c \
    kernel/serial.c \
    kernel/memory/kmalloc.c \
    kernel/block/block.c \
    kernel/block/blockdev.c \
    kernel/drivers/ata_pio.c \
    kernel/drivers/virtio_blk.c \
    kernel/drivers/kbd.c \
    kernel/drivers/ps2.c \
    kernel/drivers/vga_font.c \
    kernel/fs/vfs.c \
    kernel/fs/ramfs.c \
    kernel/fs/kvxfs.c \
    kernel/fs/toyfs.c \
    kernel/fs/toyfs_image.c \
    kernel/fs/fs_init.c \
    kernel/layout/layout.c \
    kernel/layout/us.c \
    kernel/layout/trq.c \
    lib/shell/shell.c \
    lib/commands/commands.c \
    lib/service/service.c \
    lib/service/service_registry.c \
    lib/string/string.c

# OTOMATİK KOMUT TARAMA
COMMAND_SOURCES = $(wildcard kernel/commands/*.c)
SRC_C += $(COMMAND_SOURCES)

# Nesne dosyaları listesi
OBJS = $(SRC_S:%.S=$(BUILD)/%.o) $(SRC_C:%.c=$(BUILD)/%.o)

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
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

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

# QEMU'da diski de bağlamak için "-drive" ekledim
# Yeni QEMU komutu
run: iso
	# Eğer disk.img yoksa 10MB'lık boş bir dosya oluştur (Hata almamak için)
	test -f disk.img || dd if=/dev/zero of=disk.img bs=1M count=10
	qemu-system-i386 -cdrom $(IMAGE) -drive format=raw,file=disk.img,if=ide -m 256M -serial stdio -no-reboot

clean:
	rm -rf $(BUILD) $(ISO) $(IMAGE)