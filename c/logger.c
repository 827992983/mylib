#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include "logger.h"
#include "locker.h"
#include "debug.h"

static locker_t  		g_log_mutex;
static char            *g_filename = NULL;
static FILE            *g_log_fd = NULL;
static logger_level_t   g_loglevel = LOG_ERROR;//default
logger_level_t          log_level;

static char *level_strings[] = {
	"ERROR",
	"WARNING",
	"INFO",
};

logger_level_t logger_get_level(void)
{
	return g_loglevel;
}

int logger_set_level(logger_level_t level)
{
	if(level != LOG_ERROR && level != LOG_WARNING && level != LOG_INFO)
		return -1;
	g_loglevel = level;
	return 0;
}

int logger_init (const char *file)
{
	if (!file)
	{
		DBG_PRINT("log file name is NULL!\n");
		return -1;
	}

	LOCK_INIT(&g_log_mutex);
	logger_set_level(LOG_ERROR);

	g_filename = strdup(file);
	if (!g_filename) 
	{
		DBG_PRINT("strdup error\n");
		return -1;
	}

	g_log_fd = fopen (file, "a");
	if (!g_log_fd){
		DBG_PRINT("open file:\"%s\" error!\n",file);
		return -1;
	}

	return 0;
}

void logger_cleanup(void)
{
	LOCK_DESTROY(&g_log_mutex);
	fclose(g_log_fd);
	free(g_filename);
}

int logger_fun(int level, const char *file_name, const char *func_name, const int line, const char *fmt, ...)
{
	va_list      para;
	time_t       now = 0;

	if (!fmt) 
	{
		DBG_PRINT("ERR:fmt is NULL\n");
		return -1;
	}

	if (!g_log_fd) {
		DBG_PRINT("log file is not init!\n");
		return (-1);
	}

	now = time (NULL);

	LOCK(&g_log_mutex);
	{
		if(level >= g_loglevel)
		{
			va_start (para, fmt);
			fprintf (g_log_fd, "[%ld] %s [%s:%s:%d] : ",
					now, level_strings[g_loglevel],
					file_name, func_name,line);
			vfprintf (g_log_fd, fmt, para);
			va_end (para);
			fprintf (g_log_fd, "\n");
			fflush (g_log_fd);
		}
	}
	UNLOCK(&g_log_mutex);

	return 0;
}
