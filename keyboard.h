#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/**
 * Преобразует скан-код клавиши (raw data от PS/2) в ASCII-символ.
 * Учитывает состояние служебных клавиш (например, Shift).
 * 
 * @param scancode - код, полученный из порта 0x60 (клавиатурный контроллер)
 * @return - возвращает ASCII-символ ('a', '1', 'B'...), 
 *           либо 0, если код не является печатаемым символом (например, нажатие Shift или Alt).
 */
char get_char_from_scancode(uint8_t scancode);

#endif
