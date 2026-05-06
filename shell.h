#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
//инициализация shell
void shell_init();
/**
 * Обрабатывает символ, пришедший с клавиатуры.
 * Добавляет символ в буфер команды, выводит его на экран,
 * а при нажатии Enter вызывает shell_execute.
 */
void shell_handle_char(char c);

/**
 * Парсит и выполняет введенную пользователем команду.
 * Ищет совпадения строки cmd с известными командами (ls, cd, help и т.д.).
 */
void shell_execute(char* cmd);

/**
 * Сравнивает две строки. 
 * Возвращает 0, если строки полностью идентичны.
 */
int _strcmp(const char *s1, const char *s2);

/**
 * Сравнивает первые 'n' символов двух строк.
 * Полезно для команд с аргументами (например, проверить только начало "echo ").
 */
int _strncmp(const char *s1, const char *s2, int n);


#endif

