/**
 * \file   threadpool.h
 * \author Jonathan Simmonds
 * \brief  Basic thread pool implementation with pthreads.
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sys/types.h> // pthread_t
#include <semaphore.h> // sem_t


struct thread_t
{
    struct threadpool_t* pool;
    pthread_t thread;
    void (*routine)(void*);
    void* arg;
};

/**
 * \brief   Structure representing a thread pool. Only threads_length should be
 *      interacted with by clients. Initialise with threadpool_create.
 */
typedef struct threadpool_t
{
    /** Array of thread workers. */
    struct thread_t* threads;
    /** Size of the threads array. */
    size_t threads_length;
    /** Current number of thread worker slots not in use (i.e. free). */
    sem_t threads_free;
} threadpool;

/**
 * \brief   Creates a thread pool, initialising a threadpool struct.
 *
 * \param pool  The threadpool struct to initialise.
 * \param size  The number of simultaneous threads which may run in the
 *      threadpool.
 * \return  0 on success, < 0 on error.
 */
int threadpool_create(threadpool* pool, size_t size);

/**
 * \brief   Destroys a thread pool. This must be called on the same thread the
 *      threadpool was created. Accesses to the threadpool after calling this
 *      function are undefined.
 *
 * \param pool  The initialised threadpool to destroy.
 */
void threadpool_destroy(threadpool* pool);

/**
 * \brief   Dispatches a thread in the threadpool to execute the given routine
 *      with the given argument. If no thread in the pool is currently available
 *      to dispatch to, this function will block until one is ready. This must
 *      be called on the same thread the threadpool was created.
 *
 * \param pool      The initialised threadpool to dispatch to.
 * \param routine   Pointer to the function to execute on a separate thread. Not
 *      NULL. On return the thread will be returned to the pool.
 * \param arg       Argument to pass to routine. May be NULL. If multiple
 *      arguments are needed this should be a pointer to a struct.
 * \return  0 on success, < 0 on error.
 */
int threadpool_dispatch(threadpool* pool, void (*routine)(void*), void* arg);

/**
 * \brief   Counts the number of active threads in the threadpool (i.e. threads
 *      which have been dispatched and not yet terminated).
 *
 * \param pool  The initialised threadpool whose state to query.
 * \return  The number of active threads (which may exceed pool->threads_length
 *      if there are threads waiting to dispatch). < 0 on error.
 */
int threadpool_active_threads(threadpool* pool);

#endif // THREADPOOL_H