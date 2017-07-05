#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <stdio.h>

#define DEBUG

#ifdef DEBUG
#define DBG_POSITION printf("%s:%s:%d\n",__FILE__,\
		__FUNCTION__,__LINE__)

#define DBG_PRINT(fmt...) do{\
	printf("%s:%s:%d:",__FILE__,__FUNCTION__,__LINE__);\
	printf(fmt);\
}while(0)
#else
#define DBG_POSITION
#define DBG_PRINT(fmt...)
#endif /* DEBUG */

#endif /* _DEBUG_H_H */
