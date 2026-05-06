#include "kernel.h"
#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "pmm.h"
#include "fat32.h"
#include <stdint.h>
#include "vmm.h"
#include "cpu_proc_list.h"
#include "ide.h"
#include "mboot_info.h"

/*Главный центральный файл где есть:
 * главный цикл
 * обработчик прерываний
 * менеджер процессов
*/
// указатель и число multi_boot структуры
extern uint32_t boot_magic;
extern uint32_t boot_mboot_ptr;

// переменные для менеджера процессов
//указатель на список процессов
task_t *current_task = NULL;
// указатель на следующий процесс
uint32_t next_pid = 0;
// указатель на кладбище убитых процессов
task_t *dead_list = NULL;

//прототип функции очистки
void kernel_cleaner_task();

//буфер ввода
volatile char key_buffer = 0;
//флаг состояния системного вызова
volatile uint32_t syscall_waiting = 0;

//код обработчика прерываний IDE
void ide_handler() {
    // 1. Читаем статус, чтобы диск сбросил сигнал прерывания
    inb(0x1F7); 
    
    // 2. Ставим флаг, чтобы функция чтения вышла из цикла
    ide_irq_fired = 1;

    // 3. Отправляем EOI контроллерам прерываний (обязательно!)
    //outb(0xA0, 0x20); // Slave
    //outb(0x20, 0x20); // Master
}

// Обработчик прерываний (вызывается из common_stub в boot.asm)
uint32_t exception_handler(registers_t *regs) {
    // 1. Системный вызов (int 0x30) - Самый приоритетный
    const char *hex = "0123456789ABCDEF";
    
        // КРИТИЧНО: Шлем EOI только если это IRQ (32-47)
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20); // Slave PIC
        }
        outb(0x20, 0x20);     // Master PIC
    }
    
    // системные прерывания
    if (regs->int_no == 48) {
        switch (regs->eax) {
			
			case 0: // sys_exit
				vga_puts("\n[Process] Exit requested. Killing PID: ");
				vga_put_int(current_task->id);
				vga_puts("\n");
    
				current_task->stat = 2; // Ставим TASK_STATE_KILLED
    
				// Сразу переключаемся на следующую задачу, чтобы не ждать таймера
				current_task = current_task->next;
				asm volatile("mov %0, %%cr3" :: "r"(current_task->page_directory));
				return current_task->esp; 
			
            case 1: // sys_print
                if (regs->ebx < 0x100000) vga_puts("\n[SEC] Access Denied\n");
                else vga_puts((char*)regs->ebx);
                break;
            case 2: // sys_cls
                vga_clear();
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

            case 7: //sys_write
				regs->eax = fat32_write_file((char*)regs->ebx, (uint8_t*)regs->ecx, regs->edx);
				break;
				
			case 10: { //sys_registration_process
				resource_t *res = (resource_t*)kmalloc(sizeof(resource_t));
				res->type = RES_VGA_SYMBOL;
				res->addr = regs->ebx; // номер ячейки (76-79)
				res->next = current_task->resources;
				current_task->resources = res;
				break;
			}
                
			case 3: // get_char
                // На реальном железе лучше не делать hlt внутри прерывания
                // Но если очень хочется оставить так, нужно убедиться, что IF=1
                syscall_waiting = 1;
                asm volatile("sti"); // Разрешаем прерывания, чтобы клавиатура могла сработать
                while (key_buffer == 0) { }
                regs->eax = (uint32_t)key_buffer;
                key_buffer = 0;
                syscall_waiting = 0;
                break;
            
            default:
                vga_puts("Unknown syscall\n");
                break;
        }
        return (uint32_t)regs;
    }
    
    //Прерывания IDE
    if (regs->int_no == 46) {
		// vga_puts("!!!!\n");
		ide_handler();
	}
    
    // 2. Аппаратные прерывания (IRQ)
    // также менеджер процессов
    else if (regs->int_no == 32) { // Таймер
		if (current_task != NULL) {
			/// сохранение текущего указателя стека
			current_task->esp = (uint32_t)regs;
			
			task_t *prev = current_task;
			task_t *next_t = current_task->next;

			// Идем по кольцу и выкидываем "трупы"
			while (next_t->stat == TASK_STATE_KILLED) {
				prev->next = next_t->next; // Вышвыриваем из списка
				
				next_t->next = dead_list;
				dead_list = next_t;
				
				// Если ты не используешь kfree, просто идем дальше
				next_t = prev->next;

				// Если вырезали всех и вернулись к себе — стоп
				if (next_t == prev) break;
			}
			//переключение на следующий процесс
			current_task = current_task->next;
			//обновление cr3 для vmm
			if (current_task->page_directory == 0) {
				vga_puts("CRITICAL: PD is 0 for task ");
				vga_puts(current_task->name);
				while(1) { }
			}
			
			asm volatile("mov %0, %%cr3" :: "r"(current_task->page_directory));
			//отправляем данные о принятии его сигнала
			//outb(0x20, 0x20);
			//возвращение стека новой задачи
			return current_task->esp;
		}
    }
    
    else if (regs->int_no == 33) { // Клавиатура
	    uint8_t scancode = inb(0x60);
    
		// Прямая отладка на экране 
		*((uint16_t*)0xB808E) = (uint16_t)(hex[(scancode >> 4) & 0x0F] | (0x0E << 8));
		*((uint16_t*)0xB8090) = (uint16_t)(hex[scancode & 0x0F] | (0x0E << 8));

		char c = get_char_from_scancode(scancode);
		if (c > 0) {
			if (syscall_waiting) key_buffer = c;
				else shell_handle_char(c);
		}
	}
    
    return (uint32_t)regs;
}

//процесс крутящейся палочки
void task2_test() {
    uint16_t *vga_mem = (uint16_t*)0xB8000;
    uint8_t counter = 0;
    const char *anim = "|/-\\O"; // Маленькая анимация загрузки
    
    while(1) {
        // Рисуем в самом углу (строка 0, колонка 79)
        vga_mem[79] = (uint16_t)(anim[counter % 4] | (0x0F << 8));
        counter++;
        
        // Небольшая задержка, чтобы анимация не летела слишком быстро
        for(volatile int i = 0; i < 1000000; i++); 
    }
}

//небольшая мини анимация
void task_a () {
	uint16_t *vga_mem = (uint16_t*)0xB8000;
    uint8_t counter = 0;
    const char *anim = "CRCV"; // Маленькая анимация 
    const char *anim2 = "AUAA";
    const char *anim3 = "TNTU";
    
    while(1) {
        // Рисуем в самом углу (строка 0, колонка 79)
        vga_mem[76] = (uint16_t)(anim[counter % 4] | (0x0F << 8));
        vga_mem[77] = (uint16_t)(anim2[counter % 4] | (0x0F << 8));
        vga_mem[78] = (uint16_t)(anim3[counter % 4] | (0x0F << 8));
        counter++;
        
        // Небольшая задержка, чтобы анимация не летела слишком быстро
        for(volatile int i = 0; i < 10000000; i++); 
    }
}

//функция поиска процесса в списке
task_t* find_task(uint32_t pid) {
    task_t *it = current_task;
    if (it == NULL) return NULL;

    do {
        if (it->id == pid) return it; // Нашли!
        it = it->next;
    } while (it != current_task); // Крутимся, пока не вернемся в начало

    return NULL; // Никого не нашли
}

// функция очистки по структуре ресурсов в процессе
void task_cleanup(task_t *t) {
    if (t == NULL) return;

    // 1. Очистка ресурсов (VGA символы)
    resource_t *res = t->resources;
    
    // ОГРАНИЧЕНИЕ: Если в res мусор, этот цикл улетит в бесконечность или вызовет Fault.
    // Пока у нас нет стабильной регистрации, можно временно обернуть в проверку:
    while (res != NULL) {
        // vga_put_hex((uint32_t)res); //если надо видеть адреса расскоментируй
        
        if (res->type == RES_VGA_SYMBOL) {
            uint16_t *vga_mem = (uint16_t*)0xB8000;
            // Защита: не пишем за пределы VGA буфера (80*25 = 2000 слов)
            if (res->addr < 2000) {
                vga_mem[res->addr] = 0x0720; 
            }
        }
        
        resource_t *next_res = res->next;
        kfree(res); // Раскомментируй, когда будет мелкий kfree для структур
        res = next_res;
    }

    // 2. Освобождение стека
    // ВНИМАНИЕ: проверь правильное имя поля (stack_base)
    if (t->steck_base != 0) { 
        kfree_page((void*)t->steck_base); 
        vga_puts("[Reaper] Stack memory freed for PID: ");
        vga_put_int(t->id);
        vga_puts("\n");
    }
}

// 2. Жнец (Cleaner)
void kernel_cleaner_task() {
    while(1) {
        if (dead_list != NULL) {
            // Чтобы не было проблем с прерываниями, на миг запрещаем их
            asm volatile("cli");
            task_t *victim = dead_list;
            dead_list = victim->next;
            asm volatile("sti");

            task_cleanup(victim);
            
            kfree(victim); // Окончательное удаление структуры из памяти
            vga_puts("[Reaper] Cleaned up PID: ");
            vga_put_int(victim->id);
            vga_puts("\n");
        }
        // Пауза, чтобы не грузить ядро
        for(volatile int i = 0; i < 100000; i++);
    }
}

// функция вывода о конце программы
void task_exit_trap() {
    // Выводим сообщение, чтобы понять, что программа дошла до конца
    vga_puts("\n[Process Finished] Waiting for reaper...\n");
    
    // Просто зацикливаем, чтобы процесс не прыгал по памяти
    // В идеале тут должно быть: current_task->stat = 2; (KILLED)
    while(1) {
        asm volatile("hlt");
    }
}

// Точка входа в Си-код
void kernel_main() {
	//очистка экрана от служебного вывода
    vga_clear();
    
    //2. расшифровка структуры мултибут
    unpack_multiboot(boot_magic, boot_mboot_ptr);
    
    // Проверяем, удалось ли вытащить карту памяти
    if (MMAP_PTR == 0) {
        vga_puts("Error: Memory Map (mmap) is missing!\n");
        while(1);
    }
    
    // 3. Инициализация менеджеров памяти
    //физического менеджера
    pmm_init();
    //менеджера виртуальной памяти
    vmm_init();
    //сразу тест vmm
    test_vmm();
    
    // 2. Инициализация многозадачности
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_pid++;
    current_task->page_directory = vmm_get_directory();
    current_task->resources = NULL;   // Чтобы Жнец не пошел по мусору
    current_task->steck_base = 0;    // У ядра стек статический, его нельзя удалять через kfree_page
    current_task->stat = 0;          // Состояние RUNNING
    // Заполним имя для порядка
    current_task->name[0] = 'k'; current_task->name[1] = 'e'; current_task->name[2] = 'r';
    current_task->name[3] = 'n'; current_task->name[4] = 'e'; current_task->name[5] = 'l';
    current_task->name[6] = '\0';
    current_task->next = current_task; 

    // Создаем задачи
    create_task(task2_test, current_task->page_directory, "Status_os_bar");
    create_task(task_a, current_task->page_directory, "run_CAT");
	create_task(kernel_cleaner_task, current_task->page_directory, "cleaner");
	
	//тест страничной адресации
    uint32_t cr0_val;
	asm volatile("mov %%cr0, %0" : "=r"(cr0_val));

	if (cr0_val & 0x80000000) {
		vga_puts("Paging is ENABLED. Memory is now 'Good'!\n");
	} else {
		vga_puts("Paging is DISABLED. Something went wrong...\n");
	}
	
    // 4. Запуск интерфейса (Shell)
    vga_puts("System initialized. Memory: ");
    vga_put_int(RAM_TOTAL_SIZE / (1024 * 1024));
    vga_puts(" MB\n");
    
    // инициализация драйвера фат32
    fat32_init();
    // инициализация коммандной оболочки (shell)
    shell_init();
    // включение прерываний
    asm volatile("sti");

    // 5. Основной цикл (ждем прерываний)
    while (1) {
        asm volatile("hlt");
    }
}
