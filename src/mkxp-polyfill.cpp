/*
** mkxp-polyfill.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mkxp-polyfill.h"
#include <cassert>

#if defined(MKXPZ_NO_SEMAPHORE_H) && !defined(MKXPZ_NO_PTHREAD_H)
struct mkxp_sem_private {
    unsigned int value;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};
#endif

extern "C" mkxp_thread_t mkxp_thread_self() {
#ifndef MKXPZ_NO_PTHREAD_H
    return pthread_self();
#else
    return 42;
#endif
}

extern "C" int mkxp_mutex_init(mkxp_mutex_t *mutex, bool recursive) {
#ifndef MKXPZ_NO_PTHREAD_H
    if (recursive) {
        pthread_mutexattr_t attr;
        if (pthread_mutexattr_init(&attr)) {
            return -1;
        }
        if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) {
            return -1;
        }
        return pthread_mutex_init(mutex, &attr);
    } else {
        return pthread_mutex_init(mutex, NULL);
    }
#else
    *mutex = 0;
    return 0;
#endif
}

extern "C" int mkxp_mutex_destroy(mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_PTHREAD_H
    return pthread_mutex_destroy(mutex);
#else
    assert(!*mutex);
    return 0;
#endif
}

extern "C" int mkxp_mutex_lock(mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_PTHREAD_H
    return pthread_mutex_lock(mutex);
#else
    ++*mutex;
    return 0;
#endif
}

extern "C" int mkxp_mutex_unlock(mkxp_mutex_t *mutex) {
#ifndef MKXPZ_NO_PTHREAD_H
    return pthread_mutex_unlock(mutex);
#else
    assert(*mutex);
    --*mutex;
    return 0;
#endif
}

extern "C" int mkxp_sem_init(mkxp_sem_t *sem, unsigned int value) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_init(sem, 0, value);
#elif !defined(MKXPZ_NO_PTHREAD_H)
    *sem = (void *)new mkxp_sem_private;
    int mutex_init_result = pthread_mutex_init(&((struct mkxp_sem_private *)*sem)->mutex, NULL);
    if (mutex_init_result) {
        return -1;
    }
    int cond_init_result = pthread_cond_init(&((struct mkxp_sem_private *)*sem)->cond, NULL);
    if (cond_init_result) {
        pthread_mutex_destroy(&((struct mkxp_sem_private *)*sem)->mutex);
        return -1;
    }
    ((struct mkxp_sem_private *)*sem)->value = value;
    return 0;
#else
    *sem = value;
    return 0;
#endif
}

extern "C" int mkxp_sem_destroy(mkxp_sem_t *sem) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_destroy(sem);
#elif !defined(MKXPZ_NO_PTHREAD_H)
    pthread_cond_destroy(&((struct mkxp_sem_private *)*sem)->cond);
    pthread_mutex_destroy(&((struct mkxp_sem_private *)*sem)->mutex);
    delete (struct mkxp_sem_private *)*sem;
    return 0;
#else
    return 0;
#endif
}

extern "C" int mkxp_sem_post(mkxp_sem_t *sem) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_post(sem);
#elif !defined(MKXPZ_NO_PTHREAD_H)
    while (pthread_mutex_lock(&((struct mkxp_sem_private *)*sem)->mutex)) {}
    ++((struct mkxp_sem_private *)*sem)->value;
    pthread_cond_signal(&((struct mkxp_sem_private *)*sem)->cond);
    pthread_mutex_unlock(&((struct mkxp_sem_private *)*sem)->mutex);
    return 0;
#else
    ++*sem;
    return 0;
#endif
}

extern "C" int mkxp_sem_wait(mkxp_sem_t *sem) {
#ifndef MKXPZ_NO_SEMAPHORE_H
    return sem_wait(sem);
#elif !defined(MKXPZ_NO_PTHREAD_H)
    while (pthread_mutex_lock(&((struct mkxp_sem_private *)*sem)->mutex)) {}
    for (;;) {
        if (((struct mkxp_sem_private *)*sem)->value) {
            --((struct mkxp_sem_private *)*sem)->value;
            pthread_mutex_unlock(&((struct mkxp_sem_private *)*sem)->mutex);
            return 0;
        }
        while (pthread_cond_wait(&((struct mkxp_sem_private *)*sem)->cond, &((struct mkxp_sem_private *)*sem)->mutex)) {}
    }
#else
    assert(*sem);
    --*sem;
    return 0;
#endif
}
