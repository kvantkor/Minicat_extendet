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
    mov dx, [esp + 4]    ; порт
    mov al, [esp + 8]    ; значение
    out dx, al
    ret

inb:
    mov dx, [esp + 4]
    in al, dx
    ret

; --- 16-бит (Word) ---
outw:
    mov dx, [esp + 4]
    mov ax, [esp + 8]
    out dx, ax
    ret

inw:
    mov dx, [esp + 4]
    in ax, dx
    ret

; --- 32-бит (Long/Double Word) ---
outl:
    mov dx, [esp + 4]
    mov eax, [esp + 8]
    out dx, eax
    ret

inl:
    mov dx, [esp + 4]
    in eax, dx
    ret

io_wait:
    out 0x80, al
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
        mov edi, idt_table + (8 * %1)
        mov [edi], ax
        mov word [edi+2], 0x08
        mov byte [edi+4], 0
        mov byte [edi+5], 0x8E
        shr eax, 16
        mov [edi+6], ax
    %endmacro

    SET_IDT_GATE 0, isr0
    SET_IDT_GATE 32, irq0
    SET_IDT_GATE 33, irq1    ; 32 - таймер, 33 - клавиатура
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

common_stub:
    pushad
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    push esp ; Передаем указатель на структуру registers_t
    call exception_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    popad
    add esp, 8
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
align 16
idt_table: resb 256 * 8
stack_bottom: resb 16384
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
