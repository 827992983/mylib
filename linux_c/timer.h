#ifndef _TIMER_H_
#define _TIMER_H_

#include <sys/time.h>
#include <pthread.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "locker.h"

typedef void (*timer_cbk_t)(void *);

typedef struct timer_node
{
	struct timeval val;
	timer_cbk_t cbk;
	int starting;
	locker_t *mutex;
	void *cbk_data;
}usec_timer_t;

usec_timer_t *timer_new(void);
int timer_set(usec_timer_t *priv,struct timeval *val,timer_cbk_t cbk,void *data);
int timer_start(usec_timer_t *priv);
int timer_end(usec_timer_t *priv);
void timer_cleanup(usec_timer_t *priv);

#endif /* _TIMER_H */

