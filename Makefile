CC = gcc
AS = nasm
LD = ld

#cd /home/rootcat/Desktop/загатовки/mini_cat

# Флаги
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -nostdlib
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

# ВАЖНО: boot.o должен быть ПЕРВЫМ, чтобы Multiboot header был в начале файла
OBJ = boot.o kernel.o vga.o keyboard.o pmm.o shell.o rtc.o ide.o fat32.o

all: my_os.bin

# Сборка итогового ядра
my_os.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o my_os.bin $(OBJ)

# Компиляция Си-файлов (main.c)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Компиляция Ассемблера (boot.asm)
boot.o: boot.asm
	$(AS) $(ASFLAGS) boot.asm -o boot.o

clean:
	rm -f *.o my_os.bin

run: my_os.bin
	qemu-system-i386 -kernel my_os.bin -hda disk.img
