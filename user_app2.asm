[bits 32]

[org 0x00404000]
start:
    mov edi, 0xB8000 + 150
    mov bl, 'A'
fill_loop:
    mov [edi], bl
    mov byte [edi+1], 0x0E
    mov ecx, 0x1000000
	
    ; Длинная пауза
    mov ecx, 0x5000000
    
delay_loop: 
	loop delay_loop

    inc bl
    cmp bl, 'Z'
    jle fill_loop
    jmp start

