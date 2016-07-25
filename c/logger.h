#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum 
{
	LOG_ERROR = 1,      //error
	LOG_WARNING,    	//warning
	LOG_INFO,       	//information
} logger_level_t;

extern logger_level_t log_level;

int logger_fun(int level, const char *file_name, const char *func_name ,const int line ,const char *fmt, ...);
int logger_init (const char *filename);
void logger_cleanup(void);
logger_level_t  logger_get_level (void);
int logger_set_level (logger_level_t level);

#define logger(level, fmt...) do{\
	logger_fun(level, __FILE__ ,__FUNCTION__, __LINE__, ##fmt);\
}while(0)


#endif /* __LOGGING_H__ */
