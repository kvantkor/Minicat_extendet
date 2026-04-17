#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init();
void shell_handle_char(char c);
void shell_execute(char* cmd);
int _strcmp(const char *s1, const char *s2);
int _strncmp(const char *s1, const char *s2, int n);

#endif

