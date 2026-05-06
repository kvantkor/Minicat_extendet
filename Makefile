# --- Компиляторы и инструменты ---
CC = gcc
AS = nasm
LD = ld

# Параметры mtools (пропуск проверки геометрии для ускорения записи на образ)
export MTOOLS_SKIP_CHECK=1

# --- Настройки проекта ---
KERNEL = my_os.elf
ISO = my_os.iso
ISO_DIR = iso_root
MSG = "Update MiniCat OS: implementation of FAT32 ls, run and syscalls"
GITHAB_OUT = /home/rootcat/Desktop/проекты_на_гитхаб/Minicat_extendet
VER = 2.1.0

# --- Флаги компиляции ---
# -m32: сборка под 32 бита
# -ffreestanding: ядро работает без стандартных библиотек C
# -fno-stack-protector: отключаем защиту стека (ядро еще не умеет её обрабатывать)
# -nostdlib: не использовать стандартные библиотеки при линковке
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -fno-pie -fno-pic -nostdlib -g
ASFLAGS = -f elf32
# -T linker.ld: использовать скрипт компоновки для правильного размещения секций
LDFLAGS = -m elf_i386 -T linker.ld -no-pie

# Список объектных файлов, из которых собирается ядро
OBJ = boot.o kernel.o vga.o keyboard.o pmm.o shell.o rtc.o ide.o fat32.o vmm.o cpu_proc_list.o mboot_info.o

# --- Настройки эмулятора QEMU ---
# -m 1G: выделяем 1 ГБ оперативной памяти
# -drive: подключаем образ диска с файловой системой FAT32
# -d int,cpu_reset: логировать прерывания и сбросы процессора для отладки
QEMU_FLAGS = -m 1G -cpu phenom -drive format=raw,file=disk.img -display sdl -d int,cpu_reset -D qemu.log

.PHONY: all clean run create_iso help git_upload app write_app git_version

all: $(KERNEL)

# Справка по командам сборки
help:
	@echo "=== MiniCat OS Build System ==="
	@echo "make            - Собрать только ядро (elf)"
	@echo "make run        - Запустить ядро напрямую (через Multiboot)"
	@echo "make create_iso - Создать загрузочный образ диска (.iso)"
	@echo "make app        - Собрать пользовательское приложение"
	@echo "make write_app  - Записать приложение на виртуальный диск"
	@echo "make git_upload - Выгрузка на гитхаб"
	@echo "make git_version- Создание версии проекта"

# Компоновка ядра
$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ)

# Правило для компиляции C-файлов
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Сборка ассемблерного кода загрузчика
boot.o: boot.asm
	$(AS) $(ASFLAGS) $< -o $@

# Создание загрузочного ISO образа с помощью GRUB
create_iso: $(KERNEL) write_app
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
	qemu-system-i386 $(QEMU_FLAGS) -cdrom $(ISO) -boot d

# Быстрый запуск ядра без создания ISO
run: $(KERNEL) write_app
	qemu-system-i386 $(QEMU_FLAGS) -kernel $(KERNEL)

# Сборка приложения в "чистый" бинарный код (flat binary)
app:
	$(AS) -f bin user_app.asm -o APP.BIN
	$(AS) -f bin user_app2.asm -o APP2.BIN

# Запись приложения на образ диска (используется mtools)
write_app: app
	mcopy -i disk.img -D o APP.BIN ::APP.BIN
	mcopy -i disk.img -D o APP2.BIN ::APP2.BIN

clean:
	rm -f *.o $(KERNEL) $(ISO) APP.BIN
	rm -rf $(ISO_DIR)

# Автоматизация выгрузки в GitHub
git_upload: clean
	mkdir -p $(GITHAB_OUT)
	rsync -av --exclude=.git --exclude=disk.img --exclude=*.img --exclude=*.iso ./ $(GITHAB_OUT)/
	cd $(GITHAB_OUT) && \
	git add . && \
	git commit -m '$(MSG)' && \
	git push origin main

# Создание и отправка тега версии
git_version: clean
	cd $(GITHAB_OUT) && \
	git tag -a v$(VER) -m 'Release version $(VER): $(MSG)' && \
	git push origin v$(VER)
