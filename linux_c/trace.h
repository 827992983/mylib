#ifndef _TRACE_H
#define _TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

int trace_init(const char *logdir);

int trace_get_progname(char *buf, int size);

#ifdef __cplusplus
}
#endif

#endif //_TRACE_H
