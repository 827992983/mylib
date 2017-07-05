#ifndef __LOCK_H__
#define __LOCK_H__

#define LOCK_INIT(x) CreateMutex(NULL,FALSE,x)
#define LOCK(x) WaitForSingleObject(x, INFINITE)
#define UNLOCK(x) ReleaseMutex(x)
#define LOCK_DESTROY(x) CloseHandle(x)

#endif