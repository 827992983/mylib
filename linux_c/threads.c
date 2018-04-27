#include <sys/time.h>
#include <errno.h>
#include "threads.h"
#include <stdio.h>
#include <unistd.h>

static task_queue_t *create_task_queue(void)
{
	task_queue_t *que = NULL;
	
	que = (task_queue_t *)malloc(sizeof(task_queue_t));
	if(que == NULL)
	{
		return NULL;
	}
	que->head = (struct list_node *)malloc(sizeof(struct list_node));
	if(que->head == NULL)
	{
		free(que);
		return NULL;
	}
	list_init(que->head);
	que->count = 0;

	return que;
}

static int add_new_task(task_queue_t *p_task, void *data)
{
	threads_task_t *task;

	task = (threads_task_t *)malloc(sizeof(threads_task_t));
	if(task == NULL)
	{
		return -1;
	}
	task->data = data;
	list_insert_tail(&task->node, p_task->head);
	p_task->count++;
	
	return 0;
}

static void *get_task_data(task_queue_t *p_task)
{
	threads_task_t *task;
	void *data;
	struct list_node *task_node = p_task->head->next;

	if(p_task == NULL || p_task->count <= 0)
	{
		return NULL;
	}
	task = list_entry(task_node, threads_task_t, node);
	data = task->data;
	list_remove(&task->node);
	free(task);
	p_task->count--;
	return data;
}

void thread_pool_destroy(thread_pool_t *thread_pool)
{
	if (!thread_pool || (thread_pool->state != THREAD_POOL_WORK))
	{
		return;
	}
	if (pthread_mutex_lock(&thread_pool->mutex) != 0)
	{
		exit(-1);
	}
	thread_pool->state = THREAD_POOL_EXIT;
	if (thread_pool->active > 0)
	{
		if (pthread_cond_broadcast(&(thread_pool->cond)) != 0)
		{
			pthread_mutex_unlock(&thread_pool->mutex);
			return;
		}
	}
	while (thread_pool->active > 0)
	{
		if (pthread_cond_wait (&thread_pool->cond, &thread_pool->mutex) != 0)
		{
			pthread_mutex_unlock(&thread_pool->mutex);
			return;
		}
	}
	if (pthread_mutex_unlock(&thread_pool->mutex) != 0)
	{
		exit(-1);
	}
	pthread_mutex_destroy(&(thread_pool->mutex));
	pthread_cond_destroy(&(thread_pool->cond));
	pthread_attr_destroy(&(thread_pool->attr));
	free(thread_pool);
	return;
}

thread_pool_t *thread_pool_new(int max_threads_num, int timeout, void (*handler)(void *))
{
	thread_pool_t *thread_pool = NULL;
	if (max_threads_num <= 0)
	{
		return NULL;
	}

	if(handler == NULL)
	{
		return NULL;
	}

	thread_pool = (thread_pool_t *) malloc(sizeof(thread_pool_t));
	if (!thread_pool)
	{
		free(thread_pool);
		return NULL;
	}
	
	thread_pool->number = max_threads_num;
	thread_pool->active = 0;
	thread_pool->idle = 0;
	thread_pool->timeout = timeout;
	thread_pool->handler = handler;

	if(pthread_mutex_init(&(thread_pool->mutex), NULL) != 0)
	{
		free(thread_pool);
		return NULL;
	}
	if (pthread_cond_init(&(thread_pool->cond), NULL) != 0)
	{
		free(thread_pool);
		return NULL;
	}
	if (pthread_attr_init(&(thread_pool->attr)) != 0)
	{
		free(thread_pool);
		return NULL;
	}

	if (pthread_attr_setdetachstate(&(thread_pool->attr), PTHREAD_CREATE_DETACHED) != 0)
	{
		free(thread_pool);
		return NULL;
	}
	
	thread_pool->queue = create_task_queue();
	if(thread_pool->queue == NULL)
	{
		perror("task queue malloc error!\n");
	}
	thread_pool->state = THREAD_POOL_WORK;
	return thread_pool;
}

void *thread_working(void *para)
{
	thread_pool_t *thread_pool = NULL;
	void *task_data = NULL;
	int ret, exit_flag = 0;
	struct timespec timeout;

	thread_pool = (thread_pool_t *)para;
	while(1)
	{
		if (pthread_mutex_lock(&(thread_pool->mutex)) != 0)
		{
			exit(-2);
		}
		timeout.tv_sec = thread_pool->timeout;
		timeout.tv_nsec = 0;
		thread_pool->idle++;
		while (((task_data = get_task_data(thread_pool->queue)) == NULL) 
				&& (thread_pool->state != THREAD_POOL_EXIT))
		{
			ret = pthread_cond_timedwait(&(thread_pool->cond), &(thread_pool->mutex), &timeout);
			if (ret == ETIMEDOUT)
			{
				exit_flag = 1;
				break;
			}
		}

		thread_pool->idle--;
		if (thread_pool->state == THREAD_POOL_EXIT)
		{
			exit_flag = 1;
		}
		if (pthread_mutex_unlock(&(thread_pool->mutex)) != 0)
		{
			exit(-2);
		}
		if (task_data)
		{
			thread_pool->handler(task_data);
		}
		else if (exit_flag)
		{
			break;
		}
	}
	if (pthread_mutex_lock(&(thread_pool->mutex)) != 0)
	{
		exit(-2);
	}

	thread_pool->active--;
	if (thread_pool->active == 0)
	{
		pthread_cond_broadcast(&thread_pool->cond);
	}
	if (pthread_mutex_unlock(&(thread_pool->mutex)) != 0)
	{
		exit(-2);
	}
	return NULL;
}

int thread_task_dispatch(thread_pool_t *thread_pool, void *user_data)
{
	pthread_t pth_id = 0;

	if (!thread_pool)
	{
		return -1;
	}
	if (pthread_mutex_lock(&(thread_pool->mutex)) != 0)
	{
		return -2;
	}
	if (thread_pool->state != THREAD_POOL_WORK)
	{
		if (pthread_mutex_unlock(&(thread_pool->mutex)) != 0)
		{
			return -3;
		}
		return -4;
	}

	add_new_task(thread_pool->queue, user_data);

	if ((thread_pool->idle == 0) && (thread_pool->active < thread_pool->number))
	{
		if (pthread_create(&pth_id, &(thread_pool->attr), thread_working, thread_pool) != 0)
		{
			printf("create thread error!\n");
		}
		else
		{
			thread_pool->active++;
		}
	}
	pthread_cond_signal(&(thread_pool->cond));
	if (pthread_mutex_unlock(&(thread_pool->mutex)) != 0)
	{
		return -5;
	}
	return 0;
}

#if 0
void callback(void *param)
{
	printf("thread_id=%x, param=%d\n", (int)pthread_self(), *((int *)param));
	sleep(1);
	return;
}

int main(void)
{
	int i = 0;
	thread_pool_t *thread_pool = thread_pool_new(10, 1, callback);
	printf("create thread pool\n");
	sleep(1);
	for(i = 0; i < 100; i++)
	{
		printf("add task:%d, return:%d\n", i, thread_task_dispatch(thread_pool, &i));
	}
	sleep(100);
	thread_pool_destroy(thread_pool);
}
#endif
