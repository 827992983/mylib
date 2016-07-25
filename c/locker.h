#ifndef _LOCKER_H_
#define _LOCKER_H_

#include <pthread.h>

//if the linux kernel is 2.6,using spin lock
#ifdef LINUX_2_6_KERNEL
#define LOCK_INIT(x)    pthread_spin_init (x, 0)
#define LOCK(x)         pthread_spin_lock (x)
#define TRY_LOCK(x)     pthread_spin_trylock (x)
#define UNLOCK(x)       pthread_spin_unlock (x)
#define LOCK_DESTROY(x) pthread_spin_destroy (x)
typedef pthread_spinlock_t locker_t;
#else
#define LOCK_INIT(x)    pthread_mutex_init (x, 0)
#define LOCK(x)         pthread_mutex_lock (x)
#define TRY_LOCK(x)     pthread_mutex_trylock (x)
#define UNLOCK(x)       pthread_mutex_unlock (x)
#define LOCK_DESTROY(x) pthread_mutex_destroy (x)
typedef pthread_mutex_t locker_t;
#endif /* LINUX_2_6_KERNEL */

#endif /* _LOCKER_H_ */
