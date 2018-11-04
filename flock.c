/**
 * \file   flock.c
 * \author Jonathan Simmonds
 * \brief  File locking API.
 */
#include <assert.h>     // assert
#include <fcntl.h>      // O_CREAT, O_RDWR, O_TRUNC, F_LOCK, F_ULOCK
#include <stdio.h>      // fopen, fclose, fprintf, vfprintf, fileno, fflush,
                        // fread
#include <stdlib.h>     // exit
#include <sys/stat.h>   // stat
#include <sys/types.h>  // getopt, stat
#include <unistd.h>     // getopt, lockf, gethostname, getpid, stat, access,
                        // link, lseek

#include "flock.h"


#define LOCK_MODE   (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define MAXHOSTNAME 1024
#define DIE(...) \
{ \
    fprintf(stderr, ##__VA_ARGS__); \
    fprintf(stderr, "\n"); \
    perror(__func__); \
    exit(1); \
}

int acquire_flock(flock* lock)
{
    char hostname[MAXHOSTNAME];
    pid_t pid;
    struct stat statbuf;
    assert(lock);
    assert(lock->glob_fp);

    // Does the global lock file already exist? If so we can never acquire it.
    if (access(lock->glob_fp, F_OK) == 0)
        return -1;

    // Lock implemented as per description on the open() man2 page; avoiding
    // using O_CREAT | O_EXCL as it is unsupported on early versions of NFS.

    // Form the unique lock name.
    gethostname(hostname, MAXHOSTNAME);
    pid = getpid();
    snprintf(lock->uniq_fp, MAXPATH, "%s.%s.%d", lock->glob_fp, hostname, pid);

    // Open the unique lock.
    lock->uniq_fd = open(lock->uniq_fp, O_CREAT | O_TRUNC | O_RDWR, LOCK_MODE);
    if (lock->uniq_fd < 0) DIE("Failed to open unique lock file");

    // Try to link the (global) lock file to our unique lock file.
    if (link(lock->uniq_fp, lock->glob_fp) != 0)
    {
        // Failed to create the link, use stat on the unique file to see if the
        // link count has increased to 2 (in which case we also succeeded).
        if (fstat(lock->uniq_fd, &statbuf) < 0) DIE("Failed to stat unique lock file");
        if (statbuf.st_nlink  != 2)
        {
            // Failed to link, remove the unique lock file and return.
            if (close(lock->uniq_fd) < 0) DIE("Failed to close unique lock file");
            if (remove(lock->uniq_fp) < 0) DIE("Failed to remove unique lock filen");
            return -1;
        }
    }

    // Successfully created link, lock the file until we are done initialising
    // and can advertise the daemon. As this is not done atomically with the
    // open it is possible another process can slip in, but the lockf call is
    // purely an optimisation to reduce cycles clients have to spend in a
    // spin-lock, so this is non-critical.
    lock->glob_fd = open(lock->glob_fp, O_RDWR, LOCK_MODE);
    if (lock->glob_fd < 0) DIE("Acquired but failed to open lock file");
    if (lockf(lock->glob_fd, F_LOCK, 0) < 0)
        perror("Failed to acquire r/w lock on global lock file");
    return 0;
}

void release_flock(flock* lock)
{
    assert(lock);
    assert(lock->glob_fp);
    if (lock->glob_fd > 0)
    {
        if (close(lock->glob_fd) < 0) perror("Failed to close global lock fd");
        lock->glob_fd = 0;
    }
    if (lock->uniq_fd > 0)
    {
        if (close(lock->uniq_fd) < 0) perror("Failed to close unique lock fd");
        lock->uniq_fd = 0;
    }
    if (remove(lock->glob_fp) < 0) perror("Failed to remove global lock file");
    if (remove(lock->uniq_fp) < 0) perror("Failed to remove unique lock file");
}

void post_to_flock(flock* lock, const char* msg)
{
    assert(lock);
    assert(lock->glob_fd);
    assert(msg);
    FILE* glob_fs = fdopen(lock->glob_fd, "w");
    fprintf(glob_fs, "%s\n", msg);
    fflush(glob_fs);
    lseek(lock->glob_fd, 0, SEEK_SET);
    if (lockf(lock->glob_fd, F_ULOCK, 0) < 0)
        perror("Failed to release r/w lock on global lock file");
}

void await_flock_post(char* msg, size_t msglen, flock* lock)
{
    assert(lock);
    assert(lock->glob_fp);
    assert(msg);
    assert(msglen);
    size_t read_size = 0;
    int glob_fd = open(lock->glob_fp, O_RDONLY);
    if (glob_fd < 0)
        perror("Failed to open global lock file");

    // Spin waiting for the file lock to be posted to. This uses unix file locks
    // to save spin-lock cycles, but this is purely an optimisation. As noted
    // above, it is possible for a child process to await a flock before the
    // flock initialisation has acquired the unix file lock. For that reason
    // the call is still wrapped as a spin lock.
    while (read_size == 0)
    {
        // Wait until unix file lock is released.
        if (lockf(glob_fd, F_LOCK, 0) < 0)
            perror("Failed to wait for r/w lock on global lock file");

        // Read the contents.
        read_size = read(glob_fd, msg, msglen);
        msg[msglen-1] = '\0';

        // Release the unix file lock.
        if (lockf(glob_fd, F_ULOCK, 0) < 0)
            perror("Failed to release tested r/w lock on global lock file");
    }

    close(glob_fd);
}