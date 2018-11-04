/**
 * \file   acquired.h
 * \author Jonathan Simmonds
 * \brief  Framework for a daemon process which acquires a shared resource and
 *      manages access to that resource between many processes.
 */
#include <assert.h>     // assert
#include <stdio.h>      // printf
#include <sys/types.h>  // getopt
#include <unistd.h>     // getopt, daemon

#include "flock.h"
#include "log.h"



/*
 * Defines
 */

#define DEFAULT_LOG_FILE ".acquired.log"
#define LOCK_FILE        "/tmp/.acquired.lck"
#ifndef DEBUG
  #define DEBUG 0
#endif



/*
 * Structs
 */

typedef struct cl_opts_t
{
    const char* log_file;
} cl_opts;



/*
 * Globals
 */

extern char *optarg;    // getopt
extern int optind;      // getopt
extern const char* log_file;    // log
cl_opts program_opts;



/*
 * Utility functions
 */



/*
 * Program functions
 */

void print_help(void)
{
    printf("Usage: acquired [-h] [-l LOG_FILE]\n");
    printf("\n");
    printf("Starts the daemon if necessary and prints the port number on\n");
    printf("which the daemon is listening for new connections.\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("  -h    Show this help message and exit.\n");
    printf("  -l    The log file \n");
}

void parse_command_line(cl_opts* opts, int argc, char* const argv[])
{
    int opt;
    assert(opts);

    // Set defaults.
    opts->log_file = DEFAULT_LOG_FILE;

    // Parse optional arguments.
    while ((opt = getopt(argc, argv, "hl:")) >= 0)
    {
        switch (opt)
        {
            case 'l': opts->log_file = optarg; break;
            case 'h': print_help(); exit(0); break;
            default:  print_help(); exit(1); break;
        }
    }

    // Parse positional arguments.
    // None.
}

void daemonize(void)
{
    if (daemon(1, DEBUG) < 0) DIE("Failed to daemonize");
}

void init(void)
{
    // TODO
}

void processing_loop(void)
{
    // TODO
    dlog(LOG_INFO, "done");
}

int main(int argc, char* const argv[])
{
    // Parse command line.
    parse_command_line(&program_opts, argc, argv);
    log_file = program_opts.log_file;

    // Attempt to acquire lock to ensure daemon is mutually exclusive.
    flock daemon_lock;
    daemon_lock.glob_fp = LOCK_FILE;
    if (acquire_flock(&daemon_lock) < 0)
    {
        // Daemon is already running. Wait for it to finish initialising (if it
        // still is) and return.
        await_flock_post(&daemon_lock);
        return 0;
    }

    // Do any initial setup before unblocking the parent process.
    init();

    // Advertise the process so the caller can find it.
    post_to_flock(&daemon_lock, "hello world");

    // Background the process to unblock the caller.
    daemonize();

    // Enter main processing loop.
    processing_loop();

    // Daemon finished, release lock and return.
    release_flock(&daemon_lock);
    return 0;
}
