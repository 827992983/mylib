#include "common.h"

void Char2Wchar(const char *chr, wchar_t *wchar, int size)
{
	MultiByteToWideChar( CP_ACP, 0, chr, strlen(chr)+1, wchar, size/sizeof(wchar[0]) );
}

void Wchar2Char(const wchar_t *wchar, char *chr, int length)
{
	WideCharToMultiByte( CP_ACP, 0, wchar, -1, chr, length, NULL, NULL );  
}