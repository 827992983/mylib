#ifndef _COMMON_H__
#define _COMMON_H__
#include <stdio.h>
#include <Windows.h>

void Char2Wchar(const char *chr, wchar_t *wchar, int size/*w_char buf size*/);
void Wchar2Char(const wchar_t *wchar, char *chr, int length/*char buf size*/);
void Utf8ToWchar(const char *utf8, wchar_t *wchar, int len/*wchar buf size*/);
void Unicode2Wchar(const char * str, wchar_t *result);
int ExecuteCommandAsUser(HANDLE token, char *cmd);
char * trim(char *str);
char *ReplaceChar(char *str, char _old, char _new);
char *ReplaceStr(char *src, const char *sub, const char *dst);
unsigned long CheckFileSize(const char *path);

#endif