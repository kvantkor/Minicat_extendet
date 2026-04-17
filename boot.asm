[BITS 32]

; --- Multiboot Header ---
section .multiboot
    align 4
    dd 0x1BADB002
    dd 0x03
    dd -(0x1BADB002 + 0x03)

section .text
    global _start
    global outb, inb, io_wait
    global outb, inb
	global outw, inw
	global outl, inl
	global irq1
	extern io_wait
    extern kernel_main
    extern exception_handler

_start:
    cli
    mov esp, stack_top
    
	mov [boot_magic], eax
    mov [boot_mboot_ptr], ebx
    
    ; 1. GDT
    lgdt [gdt_ptr]
    jmp 0x08:.reload_cs
.reload_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 2. PIC Remap
    call remap_pic

    ; 3. IDT Setup (заполнение исключений и таймера)
    call setup_idt
    lidt [idt_ptr]

    ; 4. PIT (100Hz)
    call setup_timer
	
    sti
    call kernel_main
    jmp $

; --- Ввод-вывод для C ---


; --- 8-бит (Byte) ---
outb:
    mov edx, [esp + 4]    ; Загружаем 32 бита, чтобы гарантированно очистить верхнюю часть EDX
    mov eax, [esp + 8]    ; Значение
    out dx, al            ; Используем только младшие 16 бит (DX) и 8 бит (AL)
    ret

inb:
    mov edx, [esp + 4]
    xor eax, eax          ; Обнуляем EAX перед чтением
    in al, dx
    ret

; --- 16-бит (Word) ---
outw:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    out dx, ax            ; Здесь используем AX
    ret

inw:
    mov edx, [esp + 4]
    xor eax, eax
    in ax, dx
    ret

; --- 32-бит (Long/Double Word) ---
outl:
    mov edx, [esp + 4]    ; Номер порта (32-битное чтение из стека, но используем DX)
    mov eax, [esp + 8]    ; 32-битное значение для записи
    out dx, eax            ; Запись двойного слова в порт
    ret

inl:
    mov edx, [esp + 4]    ; Номер порта
    in eax, dx             ; Читаем 32 бита прямо в EAX
    ret

io_wait:
    push eax              ; Сохраняем EAX, чтобы не испортить данные в C
    mov al, 0
    out 0x80, al          ; Запись в неиспользуемый порт для задержки
    pop eax
    ret

; --- Вспомогательные функции ---
remap_pic:
    mov al, 0x11 
    out 0x20, al 
    out 0xA0, al
	call io_wait
	
    mov al, 0x20 
    out 0x21, al
    call io_wait
    
    mov al, 0x28 
    out 0xA1, al
    call io_wait
    
    mov al, 0x04 
    out 0x21, al
    call io_wait
    
    mov al, 0x02 
    out 0xA1, al
    call io_wait
    
    mov al, 0x01 
    out 0x21, al 
    out 0xA1, al
    call io_wait
    
    mov al, 0x00 
    out 0x21, al  
    out 0xA1, al ; Разрешить всё
    call io_wait
    
    mov al, 0xFC ; 11111100 (биты 0 и 1 - разрешены таймер и клавиатура)
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al
    
    ret

setup_timer:
    mov al, 0x36 
    out 0x43, al
    mov ax, 11931 
    out 0x40, al 
    mov al, ah 
    out 0x40, al
    ret

setup_idt:
    ; Пример для Divide by Zero (0) и Timer (32)
	%macro SET_IDT_GATE 2
        mov eax, %2
        mov edi, idt_table    ; Загружаем базовый адрес таблицы
        add edi, (8 * %1)     ; Прибавляем смещение (номер прерывания * 8 байт)
        
        mov [edi], ax         ; Записываем нижние 16 бит адреса обработчика
        mov word [edi+2], 0x08 ; Селектор сегмента кода
        mov byte [edi+4], 0    ; Резерв
        mov byte [edi+5], 0x8E ; Флаги (Present, Ring 0, Interrupt Gate)
        shr eax, 16            ; Сдвигаем EAX, чтобы получить верхние 16 бит адреса
        mov [edi+6], ax        ; Записываем их
    %endmacro


    SET_IDT_GATE 0, isr0
    SET_IDT_GATE 32, irq0
    SET_IDT_GATE 33, irq1    ; 32 - таймер, 33 - клавиатура
    
    mov eax, isr_syscall
    mov edi, idt_table + (8 * 48)
    mov [edi], ax
    mov word [edi+2], 0x08
    mov byte [edi+4], 0
    mov byte [edi+5], 0xEE
    shr eax, 16
    mov [edi+6], ax
    ret

; --- Обработчики прерываний ---
isr0:
    push byte 0 ; dummy error code
    push byte 0 ; int number
    jmp common_stub

irq0:
    push byte 0
    push byte 32
    jmp common_stub

irq1:
    push byte 0
    push byte 33
    jmp common_stub

isr_syscall:
	push byte 0
	push byte 48
	jmp common_stub

common_stub:
	pushad      ; Сохраняет EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    push esp ; Передаем указатель на структуру registers_t
    call exception_handler
    
    mov esp, eax

    pop gs      ; Восстанавливаем сегменты в обратном порядке
    pop fs
    pop es
    pop ds      ; Теперь здесь именно то значение, которое мы сохранили
    popad
    add esp, 8  ; Пропускаем error_code и int_no
    iretd

section .data

global boot_magic
global boot_mboot_ptr
boot_magic: dd 0
boot_mboot_ptr: dd 0

align 4
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start

idt_ptr:
    dw 256 * 8 - 1
    dd idt_table

section .bss
align 16                  ; Гарантируем выравнивание в памяти

global idt_table
idt_table: resb 256 * 8

stack_bottom: 
    resb 16384            ; 16 КБ

stack_top:                ; ESP будет указывать сюда (на границу 16 байт)

section .note.GNU-stack noalloc noexec nowrite progbits
