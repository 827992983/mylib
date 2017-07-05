#include <stdarg.h>
#include <time.h>
#include "lock.h"
#include "logger.h"

#ifdef LOG_LEVEL_DEBUG
static unsigned int log_level = LOG_DEBUG;
#else
static unsigned int log_level = LOG_WARN;
#endif

static FILE *log_file = NULL;
static HANDLE log_mutex = NULL;

unsigned long get_file_size(const char *path)  
{  
	unsigned long filesize = -1;  
	FILE *fp;  
	fp = ::fopen(path, "r");  
	if(fp == NULL)  
		return filesize;  
	fseek(fp, 0L, SEEK_END);  
	filesize = ftell(fp);  
	fclose(fp);  
	return filesize;  
}  

void log_init(const char *log_file_name)
{
	unsigned long filesize = get_file_size(log_file_name);
	if (filesize > MAX_LOG_FILE_SIZE){
		DeleteFileA(log_file_name);
	}
	log_file = ::fopen(log_file_name, "a");

	if (log_file == NULL) {
		fprintf(stderr, "Failed to open log file %s\n", log_file_name);
		return;
	}

	log_mutex = LOCK_INIT(NULL);
	if(log_mutex == NULL){
		fprintf(stderr, "Failed to open log file %s\n", log_file_name);
		return;
	}
}

void log_cleanup(void)
{
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
	LOCK_DESTROY(log_mutex);
}

void log_write(unsigned int type,const char *file, const char *function, const int line, const char *format, ...)
{
    va_list para;
    const char *type_as_char[] = { "DEBUG", "WARN", "ERROR" };

	if (type < LOG_DEBUG || type > LOG_ERROR){
		fprintf(stderr, "Invalid parameter [type]\n");
		return;
	}

    if (type < log_level) {
      return;
    }

	LOCK(log_mutex);
    va_start(para, format);
    fprintf(log_file,"%ld %s %s:%s:%d pid:%d,tid:%d: ",(long)time(NULL), type_as_char[type], file, function, line, PID, TID);
    vfprintf (log_file, format, para);
    va_end(para);
	fprintf (log_file, "\n");
	fflush(log_file);
	UNLOCK(log_mutex);
	return;
}