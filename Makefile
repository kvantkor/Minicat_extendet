CC = gcc
AS = nasm
LD = ld

# Параметры mtools
export MTOOLS_SKIP_CHECK=1

# Настройки имен
KERNEL = my_os.elf
ISO = my_os.iso
ISO_DIR = iso_root
MSG = "Update MiniCat OS: implementation of FAT32 ls, run and syscalls"
GITHAB_OUT = /home/rootcat/Desktop/проекты_на_гитхаб/Minicat_extendet

# Флаги
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -fno-pie -fno-pic -nostdlib -g
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -no-pie

# Объекты ядра
OBJ = boot.o kernel.o vga.o keyboard.o pmm.o shell.o rtc.o ide.o fat32.o vmm.o cpu_proc_list.o

# Настройки QEMU для AMD V140 (Phenom наиболее близок по архитектуре)
QEMU_FLAGS = -m 1G -cpu phenom -drive format=raw,file=disk.img -display sdl

.PHONY: all clean run create_iso help git_upload app write_app

# По умолчанию просто собираем ядро
all: $(KERNEL)

# 1. Помощь
help:
	@echo "=== MiniCat OS Build System ==="
	@echo "make            - Собрать только ядро (elf)"
	@echo "make run        - Запустить текущий elf в QEMU (AMD V140)"
	@echo "make create_iso - Собрать ISO и запустить его в QEMU"
	@echo "make app        - Собрать приложение APP.BIN"
	@echo "make write_app  - Записать приложение на disk.img"
	@echo "make git_update - Очистить мусор и залить в GitHub"

# Сборка ядра
$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

boot.o: boot.asm
	$(AS) $(ASFLAGS) $< -o $@

# 2. Создание ISO и запуск
create_iso: $(KERNEL) write_app
	@echo "--- Building ISO ---"
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL) $(ISO_DIR)/boot/
	@echo "set timeout=0" > $(ISO_DIR)/boot/grub/grub.cfg
	@echo "set default=0" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "menuentry \"MiniCat OS (Release)\" {" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "    multiboot /boot/$(KERNEL)" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "    boot" >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo "}" >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)
	@rm -rf $(ISO_DIR)
	@echo "--- Launching ISO in QEMU ---"
	qemu-system-i386 $(QEMU_FLAGS) -cdrom $(ISO) -boot d

# 3. Обычный запуск (elf)
run: $(KERNEL) write_app
	qemu-system-i386 $(QEMU_FLAGS) -kernel $(KERNEL)

# Приложение
app:
	$(AS) -f bin user_app.asm -o APP.BIN

write_app: app
	mcopy -i disk.img -D o APP.BIN ::APP.BIN

clean:
	rm -f *.o $(KERNEL) $(ISO) APP.BIN
	rm -rf $(ISO_DIR)

git_upload: clean
	@echo "--- Копирование файлов в $(GITHAB_OUT) ---"
	# Создаем папку, если её нет
	mkdir -p $(GITHAB_OUT)
	# Копируем всё, кроме папки .git
	rsync -av --exclude='.git' ./ $(GITHAB_OUT)/
	@echo "--- Отправка в репозиторий Minicat_extendet ---"
	cd $(GITHAB_OUT) && \
	git add . && \
	git commit -m $(MSG) && \
	git push origin main
