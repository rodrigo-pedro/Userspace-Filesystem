/* Authors:
    Andre Luis Raposo Marinho 93687
    Rodrigo Resendes Pedro 93753    */
    
/* LOCK_H */
/* RW Lock functions. */
#include <pthread.h>

#ifndef LOCK_H
#define LOCK_H

#define LOCK_TYPE pthread_rwlock_t
#define INIT_LOCK(M) pthread_rwlock_init(M, NULL)
#define DESTROY_LOCK(M) pthread_rwlock_destroy(M)
#define SET_LOCK(M) pthread_rwlock_wrlock(M)
#define SET_LOCKRD(M) pthread_rwlock_rdlock(M)
#define SET_TRYLOCKWR(M) pthread_rwlock_trywrlock(M)
#define SET_TRYLOCKRD(M) pthread_rwlock_tryrdlock(M)
#define UNLOCK(M) pthread_rwlock_unlock(M)

void initLock(LOCK_TYPE* M);
void destroyLock(LOCK_TYPE* M);
void setLock(LOCK_TYPE* M);
void setLockRD(LOCK_TYPE* M);
int setTryLock(LOCK_TYPE* M);
void unLock(LOCK_TYPE* M);

#endif