/*
** mkxp-threads.h
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

#ifndef MKXPZ_THREADS_H
#define MKXPZ_THREADS_H

#include <stdbool.h>
#include <stdlib.h>

#ifdef MKXPZ_HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#ifdef MKXPZ_HAVE_SEMAPHORE_H
#  include <semaphore.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MKXPZ_HAVE_PTHREAD_H
typedef pthread_t mkxp_thread_t;
typedef pthread_mutex_t mkxp_mutex_t;
#else
typedef size_t mkxp_thread_t;
typedef bool mkxp_mutex_t;
#endif

#ifdef MKXPZ_HAVE_SEMAPHORE_H
typedef sem_t mkxp_sem_t;
#elif defined(MKXPZ_HAVE_PTHREAD_H)
typedef void *mkxp_sem_t;
#else
typedef unsigned int mkxp_sem_t;
#endif

mkxp_thread_t mkxp_thread_self(void);

int mkxp_mutex_init(mkxp_mutex_t *mutex);

int mkxp_mutex_destroy(mkxp_mutex_t *mutex);

int mkxp_mutex_lock(mkxp_mutex_t *mutex);

int mkxp_mutex_unlock(mkxp_mutex_t *mutex);

int mkxp_sem_init(mkxp_sem_t *sem, unsigned int value);

int mkxp_sem_destroy(mkxp_sem_t *sem);

int mkxp_sem_post(mkxp_sem_t *sem);

int mkxp_sem_wait(mkxp_sem_t *sem);

#ifdef __cplusplus
}
#endif

#endif // MKXPZ_THREADS_H
