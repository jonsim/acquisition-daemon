/**
 * \file   flock.h
 * \author Jonathan Simmonds
 * \brief  File locking API.
 */
#ifndef FLOCK_H
#define FLOCK_H

#define MAXPATH     1024

typedef struct flock_t
{
    /** Path to the global file lock. Must be populated by the constructor
     *  before passing to any methods in flock.h. */
    const char* glob_fp;
    /** Path to this process's unique file lock. */
    char        uniq_fp[MAXPATH];
    /** File descriptor of the global file lock. */
    int glob_fd;
    /** File descriptor of this process's unique file lock. */
    int uniq_fd;
} flock;

/**
 * \brief   Attempts to acquire a process-specific file lock.
 *
 * \param lock  Pointer to flock struct. Not NULL. The glob_fp field must be
 *      initialised to the file path of the file lock to acquire. All other
 *      fields should be left uninitialised.
 *
 * \return  0 on success, in which case the file lock has been acquired and the
 *      passed lock object has been populated. -1 on error, in which case the
 *      file lock could not be acquired and is already owned by another process.
 */
int acquire_flock(flock* lock);

/**
 * \brief   Releases the file lock.
 *
 * \param lock  Pointer to flock struct which has previously been acquired. Not
 *      NULL.
 */
void release_flock(flock* lock);

/**
 * \brief   Posts a message into the file lock. This can be used to pass
 *      messages to other processes which are waiting on the file lock. This
 *      neither acquires nor releases the file lock.
 *
 * \param lock  Pointer to flock struct which has previously been acquired. Not
 *      NULL.
 * \param msg   Message to write into the file lock.
 */
void post_to_flock(flock* lock, const char* msg);

/**
 * \brief   Blocks until the file lock is written to.
 *
 * \param lock  Pointer to flock struct to wait for. Not NULL. Should not be
 *      acquired (it makes no sense to call this function if this process owns
 *      the lock) the glob_fp field must be initialised to the file path of the
 *      file lock to wait on. All other fields should be left uninitialised.
 */
void await_flock_post(char* msg, size_t msglen, flock* lock);

#endif // FLOCK_H