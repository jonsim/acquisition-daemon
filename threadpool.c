/**
 * \file   threadpool.h
 * \author Jonathan Simmonds
 * \brief  Basic thread pool implementation with pthreads.
 */
#include <assert.h> // assert
#include <pthread.h> // pthread_create, pthread_self, pthread_equal
#include <stdlib.h> // malloc
#include <stdio.h> // fprintf, stderr, perror
#include <sys/types.h> // pthread_t
#include <semaphore.h> // sem_init, sem_destroy, sem_wait, sem_post, sem_getvalue

#include "threadpool.h"


#define DIE(...) \
{ \
    fprintf(stderr, ##__VA_ARGS__); \
    fprintf(stderr, "\n"); \
    perror(__func__); \
    exit(1); \
}


int threadpool_create(threadpool* pool, size_t size)
{
    assert(pool);

    pool->threads = malloc(sizeof(struct thread_t) * size);
    if (pool->threads == NULL) return -1;
    pool->threads_length = size;
    for (size_t i = 0; i < size; ++i)
    {
        pool->threads[i].pool = pool;
        pool->threads[i].routine = NULL;
        pool->threads[i].arg = NULL;
    }
    return sem_init(&pool->threads_free, 0, size);
}

void threadpool_destroy(threadpool* pool)
{
    // The destroy method must always be called on the same thread the
    // threadpool was created to ensure mutual exclusion.
    assert(pool);

    free(pool->threads);
    pool->threads = NULL;
    pool->threads_length = 0;
    sem_destroy(&pool->threads_free);
}


void* internal_dispatch(void* thread_raw)
{
    struct thread_t* thread = (struct thread_t*) thread_raw;

    // Actually run the thread routine.
    thread->routine(thread->arg);

    // Clean up the thread table to mark the slot as free.
    thread->routine = NULL;
    thread->arg = NULL;
 
    // Increment the free-slot semaphore.
    sem_post(&thread->pool->threads_free);
    return NULL;
}

int threadpool_dispatch(threadpool* pool, void (*routine)(void*), void* arg)
{
    // The dispatch method must always be called on the same thread the
    // threadpool was created to ensure mutual exclusion.
    int slot = -1;
    assert(pool);

    // Reserve an empty slot.
    if (sem_wait(&pool->threads_free) != 0) return -1;
    for (int i = 0; i < pool->threads_length; ++i)
    {
        if (pool->threads[i].routine == NULL)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -2;

    // Setup the thread table.
    pool->threads[slot].routine = routine;
    pool->threads[slot].arg = arg;

    // Dispatch the thread.
    return pthread_create(&(pool->threads[slot].thread), NULL,
        internal_dispatch, &pool->threads[slot]);
}

int threadpool_active_threads(threadpool* pool)
{
    int active_threads;
    assert(pool);

    if (sem_getvalue(&pool->threads_free, &active_threads) != 0) return -1;
    // active_threads may be < 0 if there are threads blocked waiting on the
    // semaphore. In this case the returned value will exceed threads_length.
    return pool->threads_length - active_threads;
}
