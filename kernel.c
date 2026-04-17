#include "kernel.h"
#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "pmm.h"
#include "fat32.h"
#include <stdint.h>
#include "vmm.h"
#include "cpu_proc_list.h"

extern uint32_t boot_magic;
extern uint32_t boot_mboot_ptr;

task_t *current_task = NULL;
uint32_t next_pid = 0;


volatile char key_buffer = 0;
volatile uint32_t syscall_waiting = 0;

// Обработчик прерываний (вызывается из common_stub в boot.asm)
uint32_t exception_handler(registers_t *regs) {
    // 1. Системный вызов (int 0x30) - Самый приоритетный
    const char *hex = "0123456789ABCDEF";
    
    if (regs->int_no == 48) {
        switch (regs->eax) {
            case 1:
                if (regs->ebx < 0x100000) vga_puts("\n[SEC] Access Denied\n");
                else vga_puts((char*)regs->ebx);
                break;
            case 2:
                vga_clear();
                break;
                
			case 3: // get_char
                // На реальном железе лучше не делать hlt внутри прерывания
                // Но если очень хочется оставить так, нужно убедиться, что IF=1
                syscall_waiting = 1;
                asm volatile("sti"); // Разрешаем прерывания, чтобы клавиатура могла сработать
                while (key_buffer == 0) {
                     
                }
                regs->eax = (uint32_t)key_buffer;
                key_buffer = 0;
                syscall_waiting = 0;
                break;

			case 4: // read_sector (ebx = номер сектора, ecx = адрес буфера)
				if (regs->ebx == 0) {
					vga_puts("\n[SEC] Access Denied: MBR Protection\n");
					regs->eax = -1; // Возвращаем ошибку в приложение
				} else {
					// Вызываем твой драйвер диска (функция из fat32 или disk)
					// fat32_read_sector(regs->ebx, (uint8_t*)regs->ecx);
					regs->eax = 0; // Успех
				}
				break;
            
			case 5: // create [ebx = filename_ptr]
				if (regs->ebx < 0x100000) { regs->eax = -1; break; } // Защита ядра
				regs->eax = fat32_create_file((char*)regs->ebx);
				break;

			case 6: // delete [ebx = filename_ptr]
				if (regs->ebx < 0x100000) { regs->eax = -1; break; }
				regs->eax = fat32_delete_file((char*)regs->ebx);
				break;

            case 7:
				regs->eax = fat32_write_file((char*)regs->ebx, (uint8_t*)regs->ecx, regs->edx);
				break;
            
            default:
                vga_puts("Unknown syscall\n");
                break;
        }
        return (uint32_t)regs;
    }
    // 2. Аппаратные прерывания (IRQ)
    else if (regs->int_no == 32) { // Таймер
		if (current_task != NULL) {
			/// сохранение текущего указателя стека
			current_task->esp = (uint32_t)regs;
			//переключение на следующий процесс
			current_task = current_task->next;
			//обновление cr3 для vmm
			asm volatile("mov %0, %%cr3" :: "r"(current_task->page_directory));
			//отправляем данные о принятии его сигнала
			outb(0x20, 0x20);
			//возвращение стека новой задачи
			return current_task->esp;
		}
    } else if (regs->int_no == 33) { // Клавиатура
	    uint8_t scancode = inb(0x60);
    
		// Прямая отладка на экране (как ты хотел)
		*((uint16_t*)0xB8000) = (uint16_t)(hex[(scancode >> 4) & 0x0F] | (0x0E << 8));
		*((uint16_t*)0xB8002) = (uint16_t)(hex[scancode & 0x0F] | (0x0E << 8));

		char c = get_char_from_scancode(scancode);
		if (c > 0) {
			if (syscall_waiting) key_buffer = c;
				else shell_handle_char(c);
		}
	}
    // КРИТИЧНО: Шлем EOI только если это IRQ (32-47)
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20); // Slave PIC
        }
        outb(0x20, 0x20);     // Master PIC
    }
    
    return (uint32_t)regs;
}

void task2_test() {
    uint16_t *vga_mem = (uint16_t*)0xB8000;
    uint8_t counter = 0;
    const char *anim = "|/-\\"; // Маленькая анимация загрузки
    
    while(1) {
        // Рисуем в самом углу (строка 0, колонка 79)
        vga_mem[79] = (uint16_t)(anim[counter % 4] | (0x0F << 8));
        counter++;
        
        // Небольшая задержка, чтобы анимация не летела слишком быстро
        for(volatile int i = 0; i < 1000000; i++); 
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
	uint32_t flags = mboot[0];

	uint32_t total_mem_bytes;

	if (flags & 0x1) {
		uint32_t mem_upper = mboot[2];
		total_mem_bytes = (mem_upper * 1024) + (1024 * 1024);
	} else {
		// Если флаг не установлен, используем безопасный минимум (например, 16МБ)
		// чтобы ядро хотя бы загрузилось
		total_mem_bytes = 16 * 1024 * 1024; 
	}
	
    // 3. Инициализация менеджеров памяти
	
    pmm_init(total_mem_bytes);
    vmm_init();
    
    // 2. Инициализация многозадачности
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_pid++;
    current_task->page_directory = vmm_get_directory();
    // Заполним имя для порядка
    current_task->name[0] = 'k'; current_task->name[1] = 'e'; current_task->name[2] = 'r';
    current_task->name[3] = 'n'; current_task->name[4] = 'e'; current_task->name[5] = 'l';
    current_task->name[6] = '\0';
    current_task->next = current_task; 

    // Создаем вторую задачу
    create_task(task2_test, current_task->page_directory, "task2");

    uint32_t cr0_val;
	asm volatile("mov %%cr0, %0" : "=r"(cr0_val));

	if (cr0_val & 0x80000000) {
		vga_puts("Paging is ENABLED. Memory is now 'Good'!\n");
	} else {
		vga_puts("Paging is DISABLED. Something went wrong...\n");
	}
	
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

/*
Это отличная точка для завершения этапа. Мы подняли MiniCat OS до уровня «тяжеловеса»: 100 ГБ диск подключен, ядро видит ФС, а драйверы IDE и памяти стабильно работают. Проблема со сдвигом данных на «гиганте» — это классическая задача по отладке геометрии диска, которую мы решим в следующий раз.Ниже подготовлен структурированный Промт-контейнер. Просто скопируй его и вставь в новое окно чата, когда будешь готов продолжать. Он содержит весь контекст проекта, текущий статус и технические детали.Промт для продолжения разработки MiniCat OSРоль: Ты — опытный системный разработчик (ОС на C и NASM). Мы разрабатываем 32-битную учебную ОС MiniCat OS.Стек: GCC (-m32, -ffreestanding), NASM (elf32), QEMU.Текущий статус проекта:Ядро: Multiboot, GDT, IDT (int 0x30 syscalls DPL=3), PMM (Bitmap), kmalloc, RTC, PS/2 Keyboard (с поддержкой Shift).Дисковая подсистема: IDE драйвер (LBA28). Реализован гигантский диск 100 ГБ (100 GiB), отформатированный в FAT32 (superfloppy mode, mkfs.fat32 -I).Файловая система: Реализованы функции init, list_root, find, read, create, delete (со шредером/обнулением кластеров) и write.Интерфейс: Shell с базовыми командами и запуском бинарников (run [file]).Текущая техническая проблема: При монтировании образа в Linux (AntiX) и записи файла APP.BIN, ядро MiniCat OS видит в первом байте корневого каталога 229 (0xE5) (метка удаления), хотя файл был записан с sync.Логи отладки последнего запуска на 100 ГБ диске:data_start_sector: 51264sectors_per_cluster: 64root_cluster: 2root_lba: 51264signature: 0x55AA (OK)FS Type: FAT32 (OK)Задача на следующую сессию:Выяснить причину рассинхронизации адресов между Linux и ядром на дисках > 32 ГБ (проверка расчета data_start_sector и возможных скрытых секторов).Проверить работоспособность fat32_write_file через системный вызов №7.Перейти к реализации инсталлятора системы на диск или многозадачности.Инструкция для ИИ: Продолжай в лаконичном стиле, предлагай конкретные правки в код. Начни с анализа того, почему на 100 ГБ диске root_lba может указывать на удаленную запись, когда Linux видит её как живую.
*/
