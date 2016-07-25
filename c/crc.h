#ifndef _CRC_H_
#define _CRC_H_
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

unsigned short crc8(unsigned char *ptr, unsigned int len);
unsigned short crc16(unsigned char *ptr, unsigned int len);
unsigned int crc32(unsigned char *ptr, unsigned int len);
#ifdef __cplusplus
}
#endif

#endif
