#include "kernel.h"
#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "pmm.h"

/*
«Привет! Мы разрабатываем 32-битную учебную ОС (x86) на языке C и NASM. 
Мне нужна помощь в продолжении разработки.
Текущий статус проекта:Загрузчик: Multiboot-совместимый (boot.asm), работает в QEMU через -kernel.Ядро: Работает в защищенном режиме. 
Настроены GDT, IDT (исключения и аппаратные прерывания), PIC Remap и системный таймер PIT.
Память: Реализован физический менеджер (PMM) на базе Bitmap. 
Есть базовая куча (kmalloc) для динамического выделения. 
Данные о памяти (mem_upper) передаются из Multiboot.Периферия: Драйвер VGA (текстовый режим 80x25), драйвер клавиатуры (PS/2), драйвер CMOS (RTC часы) и драйвер диска ATA PIO (чтение секторов LBA).Интерфейс: Модульный Shell (shell.c) с поддержкой команд (help, cls, mem, time, disk).
Файловая система: Начата реализация FAT32. Драйвер IDE успешно читает 0-й сектор, BPB парсится, название ФС подтверждено.
Стек технологий:GCC (флаги: -m32 -ffreestanding -nostdlib -fno-stack-protector).
NASM (elf32).LD (с кастомным linker.ld, точка входа _start, база 1M).
Следующая задача:
Реализовать полноценный листинг файлов (команда ls) через чтение корневого каталога FAT32 (Root Directory) и функцию поиска/чтения содержимого файлов по кластерам.
Помоги реализовать структуру fat32_entry_t и логику обхода кластерной цепи в fat32.c.
 Продолжаем в том же лаконичном стиле.»
*/

extern uint32_t boot_magic;
extern uint32_t boot_mboot_ptr;

// Обработчик прерываний (вызывается из common_stub в boot.asm)
void exception_handler(registers_t *regs) {
    // 1. Обработка Таймера (IRQ 0)
    if (regs->int_no == 32) {
        outb(0x20, 0x20); // Шлем EOI (End of Interrupt)
    } 
    
    // 2. Обработка Клавиатуры (IRQ 1)
    else if (regs->int_no == 33) {
        uint8_t scancode = inb(0x60); // Читаем порт клавиатуры
        
        if (!(scancode & 0x80)) { // Если клавиша нажата (не отпущена)
            char c = get_char_from_scancode(scancode);
            if (c > 0) {
                shell_handle_char(c); // Передаем символ в Shell
            }
        }
        outb(0x20, 0x20); // Шлем EOI
    }
    
    // 3. Обработка Исключений (например, деление на ноль)
    else if (regs->int_no < 32) {
        vga_clear();
        vga_puts("KERNEL PANIC! Exception: ");
        vga_put_int(regs->int_no);
        while(1); // Зависаем
    }
}


// Точка входа в Си-код
void kernel_main() {
    vga_clear();
    
    // 1. Проверка Multiboot Magic
    if (boot_magic != 0x2BADB002) {
        vga_puts("Error: Invalid Multiboot Magic!\n");
        while(1);
    }

    // 2. Определение объема памяти из структуры Multiboot
    uint32_t* mboot = (uint32_t*)boot_mboot_ptr;
    uint32_t mem_upper = mboot[2]; // Смещение 8 байт (mem_upper)
    uint32_t total_mem_bytes = (mem_upper * 1024) + (1024 * 1024);

    // 3. Инициализация менеджеров памяти
    pmm_init(total_mem_bytes);
    
    // 4. Запуск интерфейса (Shell)
    vga_puts("System initialized. Memory: ");
    vga_put_int(total_mem_bytes / (1024 * 1024));
    vga_puts(" MB\n");
    
    fat32_init();
    shell_init();

    // 5. Основной цикл (ждем прерываний)
    while (1) {
        asm volatile("hlt");
    }
}
