#include "common.h"

#ifdef _WIN32
void mutex_lock(mutex_t *lock) { EnterCriticalSection(lock); }

void mutex_unlock(mutex_t *lock) { LeaveCriticalSection(lock); }

void mutex_init(mutex_t *lock) { InitializeCriticalSection(lock); }

void mutex_init_recursive(mutex_t *lock) { InitializeCriticalSection(lock); }

void mutex_destroy(mutex_t *lock) { DeleteCriticalSection(lock); }
#else
void mutex_lock(mutex_t *lock) { pthread_mutex_lock(lock); }

void mutex_unlock(mutex_t *lock) { pthread_mutex_unlock(lock); }

void mutex_init(mutex_t *lock) { pthread_mutex_init(lock, NULL); }

void mutex_init_recursive(mutex_t *lock) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);

#ifdef PTHREAD_MUTEX_RECURSIVE
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#else
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#endif
  pthread_mutex_init(lock, &attr);
  pthread_mutexattr_destroy(&attr);
}

void mutex_destroy(mutex_t *lock) { pthread_mutex_destroy(lock); }
#endif
