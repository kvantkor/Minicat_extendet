[bits 32]
org 0x300000

start:
    ; --- ШАГ 1: Создаем файл ---
    mov eax, 5          ; Syscall: create_file
    mov ebx, filename   ; "TEST.TXT"
    int 0x30
    
    ; --- ШАГ 2: Записываем данные в файл ---
    mov eax, 7          ; Syscall: write_file
    mov ebx, filename   ; Имя файла
    mov ecx, file_data  ; Что пишем
    mov edx, data_len   ; Сколько байт (19)
    int 0x30
    
    ; --- ШАГ 3: Проверяем результат (принт сообщения) ---
    mov eax, 1          ; Syscall: print
    mov ebx, msg_done
    int 0x30

    ret

section .data
filename  db "TEST.TXT", 0
file_data db "Hello from ASM App!", 0
data_len  equ $ - file_data

msg_done  db 0x0A, "Process finished. Type 'cat TEST.TXT' in shell.", 0x0A, 0
