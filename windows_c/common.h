#ifndef _COMMON_H__
#define _COMMON_H__
#include <stdio.h>
#include <Windows.h>

void Char2Wchar(const char *chr, wchar_t *wchar, int size/*w_char buf size*/);
void Wchar2Char(const wchar_t *wchar, char *chr, int length/*char buf size*/);

#endif