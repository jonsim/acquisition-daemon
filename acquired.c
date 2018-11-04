/**
 * \file   acquired.h
 * \author Jonathan Simmonds
 * \brief  Framework for a daemon process which acquires a shared resource and
 *      manages access to that resource between many processes.
 */
#include <assert.h>     // assert
#include <netinet/in.h> // sockaddr_in
#include <stdio.h>      // printf
#include <sys/socket.h> // socket, bind, listen, getsockname
#include <unistd.h>     // getopt, daemon

#include "flock.h"
#include "log.h"



/*
 * Defines
 */
#ifndef DEBUG
  #define DEBUG 0
#endif

#define DEFAULT_LOG_FILE ".acquired.log"
#define LOCK_FILE        "/tmp/.acquired.lck"
#define FLOCK_POST_LEN   128



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

int init(void)
{
    int listen_fd;
    struct sockaddr_in listen_addr;
    
    // Create the main listening socket.
    // TODO: AF_LOCAL?
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) DIE("Failed to create socket");

    // Bind the listening socket;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listen_addr.sin_port = htons(0);
    if (bind(listen_fd, (struct sockaddr*) &listen_addr, sizeof(listen_addr)) < 0)
        DIE("Failed to bind socket");

    // Listen for connections.
    if (listen(listen_fd, 64) < 0) DIE("Failed to listen socket");

    return listen_fd;
}

void get_port(char* port_s, int socket_fd)
{
    unsigned int port;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(socket_fd, (struct sockaddr*) &addr, &addr_len) < 0)
        DIE("Failed to retrieve socket's bound port");

    port = ntohs(addr.sin_port);
    snprintf(port_s, 10, "%d", port);
}

void processing_loop(int listen_fd)
{

    dlog(LOG_INFO, "Processing finished, exiting");
}

int main(int argc, char* const argv[])
{
    int listen_fd;
    char port_s[10];
    char flock_msg[FLOCK_POST_LEN];

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
        dlog(LOG_INFO, "Daemon already running, awaiting initialisation...");
        await_flock_post(flock_msg, FLOCK_POST_LEN, &daemon_lock);
        dlog(LOG_INFO, "Daemon up on port %s", flock_msg);
        printf("%s\n", flock_msg);
        return 0;
    }
    dlog(LOG_INFO, "No daemon running, lock acquired, initialising...");

    // Do any initial setup before unblocking the parent process.
    listen_fd = init();
    get_port(port_s, listen_fd);

    // Advertise the process so the caller can find it.
    post_to_flock(&daemon_lock, port_s);
    dlog(LOG_INFO, "Daemon up on port %s", port_s);
    printf("%s\n", port_s);

    // Background the process to unblock the caller.
    daemonize();

    // Enter main processing loop.
    processing_loop(listen_fd);

    // Daemon finished, release lock and return.
    release_flock(&daemon_lock);
    return 0;
}
