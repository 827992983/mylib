#ifndef __THREADS_H__
#define __THREADS_H__

#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "list.h"

typedef struct threads_task
{
	struct list_node node;
	void *data;
} threads_task_t;

typedef struct task_queue
{
	struct list_node *head;
	int count;
} task_queue_t;

typedef enum
{
	THREAD_POOL_NONE,
	THREAD_POOL_WORK,
	THREAD_POOL_EXIT,
} thread_pool_state;

typedef struct thread_pool
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_attr_t attr;
	thread_pool_state state;
	int number;
	int active;
	int idle;
	int timeout;
	void (*handler)(void *);
	task_queue_t *queue;
} thread_pool_t;

thread_pool_t *thread_pool_new(int max_threads_num, int timeout, void (*handler)(void *));//timeout is second
void thread_pool_destroy(thread_pool_t *thread_pool);
int thread_task_dispatch(thread_pool_t *thread_pool, void *user_data);

#endif
