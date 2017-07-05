#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <sys/uio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#define get_min(a,b) ((a)<(b)?(a):(b))
#define get_max(a,b) ((a)>(b)?(a):(b))
#define get_roof(a,b) ((((a)+(b)-1)/((b)?(b):1))*(b))
#define get_floor(a,b) (((a)/((b)?(b):1))*(b))


#define SIZE_KB    1024ULL
#define SIZE_MB    1048576ULL
#define SIZE_GB    1073741824ULL
#define SIZE_TB    1099511627776ULL
#define SIZE_PB    1125899906842624ULL

#define ERR_ABORT(ptr)				\
	if (ptr == NULL)  {			\
		abort ();			\
	}                     


char *trim (char *string);
int strsplit (const char *str, const char *delim,  char ***tokens, int *token_count);

#endif /* _UTILS_H */

