#include "timer.h"

#define TIME_TO_USECONDND(val) ((((unsigned long long) val.tv_sec) * 1000000) + (val.tv_usec))

void *deal_timer(void *data)
{
	struct timeval now_time;
	struct timeval run_time;
	unsigned long long usec;
	int flag = 0;
	usec_timer_t *priv = (usec_timer_t *)data;

	usec = TIME_TO_USECONDND(priv->val);
	if(usec >= 10)
		flag =1;
	while(priv->starting)
	{
		gettimeofday(&now_time,NULL);
		while(1)
		{
			if(flag)
			{
				usleep(10);
			}
			gettimeofday(&run_time,NULL);
			if(TIME_TO_USECONDND(run_time) - TIME_TO_USECONDND(now_time) >= usec)
			{
				LOCK(priv->mutex);
				priv->cbk(priv->cbk_data);
				UNLOCK(priv->mutex);
				break;
			}
		}
	}
	return NULL;
}

usec_timer_t *timer_new(void)
{
	usec_timer_t *priv = NULL;

	priv = (usec_timer_t *)malloc(sizeof(usec_timer_t));
	priv->val.tv_sec = 0; //seconds
	priv->val.tv_usec = 0;//mini seconds
	priv->starting = 0;
	priv->cbk = NULL;
	priv->mutex = (locker_t *)malloc(sizeof(locker_t));
	LOCK_INIT(priv->mutex);
	return priv;
}
int timer_set(usec_timer_t *priv,struct timeval *val,timer_cbk_t cbk,void *data)
{
	if(priv == NULL || val == NULL || cbk == NULL)
	{
		return -1;
	}
	priv->val.tv_sec = val->tv_sec;
	priv->val.tv_usec = val->tv_usec;
	priv->cbk = cbk;
	priv->cbk_data = data;
	return 0;
}
void timer_cleanup(usec_timer_t *priv)
{
	LOCK_DESTROY(priv->mutex);
	free((void *)priv->mutex);
	free(priv);
}
int timer_start(usec_timer_t *priv)
{
	pthread_t pid;
	int ret;

	priv->starting = 1;
	ret = pthread_create(&pid,NULL,deal_timer,(void *)priv);
	if(ret != 0)
	{
		priv->starting = 0;
		return -1;
	}
	return 0;
}

int timer_end(usec_timer_t *priv)
{
	LOCK(priv->mutex);
	priv->starting = 0;
	UNLOCK(priv->mutex);
}
