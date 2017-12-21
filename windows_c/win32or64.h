#ifndef _WIN32OR64_H__
#define _WIN32OR64_H__
#include <Windows.h>
BOOL Is64BitOS();
BOOL Is64BitPorcess(DWORD dwProcessID);
#endif