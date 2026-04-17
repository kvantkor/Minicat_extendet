#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>

// Явно задаем размер 128, чтобы избежать вылета за пределы
static const char kbd_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // До 0x3A (Caps) тут теперь нули
};

static const char kbd_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

extern void shell_handle_char(char c);
static int shift_active = 0;
extern void keyboard_handler();


char get_char_from_scancode(uint8_t scancode) {
    // Нажатие Shift (Left = 0x2A, Right = 0x36)
    if (scancode == 0x2A || scancode == 0x36) {
        shift_active = 1;
        return 0;
    }
    // Отпускание Shift (Break-коды: 0xAA, 0xB6)
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_active = 0;
        return 0;
    }

    // Если это Break-код любой другой клавиши (бит 7 установлен) — игнорируем
    if (scancode & 0x80) return 0;
    
    // Защита от выхода за пределы массива
    if (scancode >= 128) return 0;

    return shift_active ? kbd_map_shift[scancode] : kbd_map[scancode];
}
