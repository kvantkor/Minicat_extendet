[BITS 32]

; --- Multiboot Header ---
; Стандарт для загрузчиков вроде GRUB. Позволяет ядру загружаться корректно.
section .multiboot
    align 4
    dd 0x1BADB002             ; Magic number
    dd 0x03                   ; Flags (запрашиваем информацию о памяти и модулях)
    dd -(0x1BADB002 + 0x03)   ; Checksum

section .text
    global _start
    global outb, inb, io_wait
    global outw, inw
    global outl, inl
    global irq1
    extern io_wait
    extern kernel_main
    extern exception_handler

_start:
    cli                       ; Отключаем прерывания на время инициализации GDT и IDT
    mov esp, stack_top        ; Устанавливаем указатель стека
    
    ; Сохраняем данные от загрузчика (EAX - magic, EBX - mboot struct ptr)
    mov [boot_magic], eax
    mov [boot_mboot_ptr], ebx
    
    ; 1. GDT (Global Descriptor Table)
    ; Настройка сегментации. Переходим из "грязного" состояния загрузчика в чистое 32-битное.
    lgdt [gdt_ptr]
    jmp 0x08:.reload_cs       ; Длинный прыжок для обновления регистра CS
.reload_cs:
    mov ax, 0x10              ; 0x10 - селектор данных в твоей GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 2. PIC Remap (Контроллер прерываний)
    ; По умолчанию прерывания железа (IRQ) пересекаются с исключениями CPU.
    ; Мы сдвигаем IRQ 0-15 на векторы 32-47.
    call remap_pic

    ; 3. IDT Setup (Interrupt Descriptor Table)
    ; Регистрируем обработчики событий (ошибки, таймер, системные вызовы).
    call setup_idt
    lidt [idt_ptr]

    ; 4. PIT (Programmable Interval Timer)
    ; Настраиваем таймер на частоту ~100 Гц.
    call setup_timer
	
    sti                       ; Включаем прерывания обратно
    call kernel_main          ; Переходим в C-код
    jmp $                     ; Вечный цикл, если вышли из kernel_main

; --- I/O Functions ---
; Используются в C для общения с портами (клавиатура, диск, видеокарта).

outb: ; void outb(uint16_t port, uint8_t data)
    mov edx, [esp + 4]    ; Номер порта
    mov eax, [esp + 8]    ; Данные
    out dx, al            ; Вывод байта
    ret

inb: ; uint8_t inb(uint16_t port)
    mov edx, [esp + 4]
    xor eax, eax
    in al, dx             ; Чтение байта
    ret

; Аналогично для Word (16 бит) и Long (32 бита)
outw:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    out dx, ax
    ret

inw:
    mov edx, [esp + 4]
    xor eax, eax
    in ax, dx
    ret

outl:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    out dx, eax
    ret

inl:
    mov edx, [esp + 4]
    in eax, dx
    ret

io_wait:
    ; Маленькая пауза для старого железа, чтобы порт успел обработать запрос.
    push eax
    mov al, 0
    out 0x80, al          ; Запись в "мусорный" порт
    pop eax
    ret

; --- Подготовка железа ---

remap_pic:
    ; Инициализация PIC1 и PIC2 (каскадный режим)
    mov al, 0x11 
    out 0x20, al 
    out 0xA0, al
    call io_wait
	
    ; Установка смещения: IRQ 0-7 -> 0x20 (32), IRQ 8-15 -> 0x28 (40)
    mov al, 0x20 
    out 0x21, al
    mov al, 0x28 
    out 0xA1, al
    call io_wait
    
    ; Настройка связки Master/Slave
    mov al, 0x04 
    out 0x21, al
    mov al, 0x02 
    out 0xA1, al
    call io_wait
    
    ; Режим 8086
    mov al, 0x01 
    out 0x21, al 
    out 0xA1, al
    call io_wait
    
    ; Маскирование: разрешаем только таймер (IRQ0) и клавиатуру (IRQ1)
    mov al, 0xF8 ; 11111000b
    out 0x21, al
    
    ;Slave pic
    mov al, 0x00  ;3F
    out 0xA1, al
    ret

setup_timer:
    ; Частота 1.193182 MHz / 11931 ~= 100 Гц
    mov al, 0x36 
    out 0x43, al
    mov ax, 11931 
    out 0x40, al 
    mov al, ah 
    out 0x40, al
    ret

setup_idt:
    ; Макрос для заполнения записи в таблице прерываний
    %macro SET_IDT_GATE 2
        mov eax, %2
        mov edi, idt_table
        add edi, (8 * %1)
        
        mov [edi], ax         ; Адрес (0-15 биты)
        mov word [edi+2], 0x08 ; Селектор кода (из GDT)
        mov byte [edi+4], 0    ; Reserved
        mov byte [edi+5], 0x8E ; Флаги: Present, Ring 0, Interrupt Gate
        shr eax, 16
        mov [edi+6], ax        ; Адрес (16-31 биты)
    %endmacro

    SET_IDT_GATE 0, isr0     ; Divide by Zero
    SET_IDT_GATE 32, irq0    ; PIT Timer
    SET_IDT_GATE 33, irq1    ; Keyboard
    SET_IDT_GATE 46, irq14    ;DISK1
    SET_IDT_GATE 47, irq15    ;DISK2
    
    ; Системный вызов (int 0x30 / 48) - ставим флаги 0xEE для доступа из User Mode
    SET_IDT_GATE 48, isr_syscall
    ret

; --- Обработка прерываний ---

; Общая "заглушка" (Stub)
; Сохраняет состояние процессора и вызывает обработчик на C
common_stub:
    pushad      ; Сохраняем регистры общего назначения
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10 ; Загружаем сегмент данных ядра
    mov ds, ax
    mov es, ax
    
    push esp     ; Передаем указатель на стек (структура registers_t в C)
    call exception_handler
    
    ; exception_handler может вернуть новый ESP (для переключения контекста)
    mov esp, eax

    pop gs
    pop fs
    pop es
    pop ds
    popad
    add esp, 8   ; Очистка стека от error_code и int_no
    iretd        ; Возврат из прерывания

; Индивидуальные входы прерываний
	isr0:
	push 0 
	push 0 
	jmp common_stub
	
	irq0: 
	push 0 
	push 32
	jmp common_stub
	
	irq1: 
	push 0 
	push 33 
	jmp common_stub
	
	irq14:
	push 0
	push 46
	jmp common_stub
	
	irq15:
	push 0
	push 47
	jmp common_stub
	
	isr_syscall: 
	push 0 
	push 48
	jmp common_stub

section .data
global boot_magic
global boot_mboot_ptr
boot_magic: dd 0
boot_mboot_ptr: dd 0

align 4
gdt_start:
    dq 0x0000000000000000 ; Null descriptor
    dq 0x00CF9A000000FFFF ; Code segment (0..4GB, Exec/Read, Ring 0)
    dq 0x00CF92000000FFFF ; Data segment (0..4GB, Read/Write, Ring 0)
gdt_end:
gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start

idt_ptr:
    dw 256 * 8 - 1
    dd idt_table

section .bss
align 16
global idt_table
idt_table: resb 256 * 8

stack_bottom: 
    resb 16384 ; 16 KB
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
