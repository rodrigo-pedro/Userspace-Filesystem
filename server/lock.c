/* Authors:
    Andre Luis Raposo Marinho 93687
    Rodrigo Resendes Pedro 93753    */

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include "lock.h"

/* Functions which verify errors in mutex and rwlock operations */

void initLock(LOCK_TYPE* M) {
    if(INIT_LOCK(M)) {
        perror("Error initializing mutex.\n");
        exit(EXIT_FAILURE);
    }
}

void destroyLock(LOCK_TYPE* M) {
    if (DESTROY_LOCK(M)) {
        perror("Error destroying mutex.\n");
        exit(EXIT_FAILURE);
    }
}

void setLock(LOCK_TYPE* M) {
    if (SET_LOCK(M)) {
        perror("Error locking mutex.\n");
        exit(EXIT_FAILURE);
    }
}

void setLockRD(LOCK_TYPE* M) {
    if (SET_LOCKRD(M)) {
        perror("Error locking RD.\n");
        exit(EXIT_FAILURE);
    }
}

int setTryLock(LOCK_TYPE* M) {
    int err = SET_TRYLOCKWR(M);
    if(err != EBUSY && err != 0) {
        perror("Trylock error.\n");
    }
    return err;
}


void unLock(LOCK_TYPE* M) {
    if (UNLOCK(M)) {
        perror("Error unlocking.\n");
        exit(EXIT_FAILURE);
    }
}