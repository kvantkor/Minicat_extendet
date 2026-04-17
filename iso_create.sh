#!/bin/bash

# Настройки
KERNEL="my_os.elf"     # Имя твоего бинарника
ISO_NAME="my_os.iso"    # Имя выходного файла

# Создаем временную структуру
mkdir -p iso_root/boot/grub

# Создаем конфиг GRUB на лету
cat << EOF > iso_root/boot/grub/grub.cfg
set timeout=0
set default=0

menuentry "My OS" {
    multiboot /boot/$KERNEL
    boot
}
EOF

# Копируем ядро
cp $KERNEL iso_root/boot/

# Главная магия
grub-mkrescue -o $ISO_NAME iso_root

# Подчищаем за собой
rm -rf iso_root

echo "Готово! Образ $ISO_NAME собран."
