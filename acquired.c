/**
 * \file   acquired.c
 * \author Jonathan Simmonds
 * \brief  Framework for a daemon process which acquires a shared resource and
 *      manages access to that resource between many processes.
 */
#include <assert.h>     // assert
#include <netinet/in.h> // sockaddr_in
#include <poll.h>       // poll, struct pollfd
#include <pthread.h>    // pthread_create, pthread_t
#include <stdio.h>      // printf
#include <string.h>     // strncmp
#include <sys/socket.h> // socket, bind, listen, getsockname
#include <unistd.h>     // getopt, daemon

#include "flock.h"
#include "log.h"
#include "threadpool.h"



/*
 * Defines
 */
#ifndef DEBUG
  #define DEBUG 0
#endif

#define DEFAULT_LOG_FILE    ".acquired.log"
#define LOCK_FILE           "/tmp/.acquired.lck"
#define FLOCK_POST_LEN      128
#define SERVER_QUEUE        64
#define SERVER_TIMEOUT      10 * 1000 // milliseconds
#define SERVER_BUFLEN       1024
#define SERVER_THREADS      64



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
 * Functions
 */

/**
 * \brief   Prints the program help text.
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
    printf("  -l    Path to the log file to use.\n");
}

/**
 * \brief   Parses the command line arguments.
 *
 * \param opts  Pointer to the cl_opts structure to populate with arguments.
 * \param argc  The number of passed arguments.
 * \param argv  The passed argument array.
 */
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

/**
 * \brief   Daemonize's the process. The parent process will successfully exit
 *      upon calling this function.
 */
void daemonize(void)
{
    // Ensure all standard streams are flushed before daemonizing.
    fflush(stdout);
    // Daemonize.
    if (daemon(1, 0) < 0) DIE("Failed to daemonize");
}

/**
 * \brief   Initialises the server's networking.
 *
 * \return  The file descriptor on which the server is listening for clients.
 */
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
    if (listen(listen_fd, SERVER_QUEUE) < 0) DIE("Failed to listen socket");

    return listen_fd;
}

/**
 * \brief   Determines the port a socket is listening on.
 *
 * \param port_s    Pointer to buffer in which the port number will be written.
 *      This must be large enough to hold an unsigned integer (10 characters).
 *      Not NULL.
 * \param socket_fd Socket file descriptor whose port to determine.
 */
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

/**
 * \brief   Processes a connection with a client.
 *
 * \param client_fd File descriptor of the client to process.
 */
void process_connection(void* client_fd_raw)
{
    int client_fd = (long) client_fd_raw;
    int ret;
    char rdbuf[SERVER_BUFLEN];
    char wrbuf[SERVER_BUFLEN];

    // Read the command.
    while ((ret = read(client_fd, rdbuf, SERVER_BUFLEN)) == 0) {}
    if (ret <= 0)
    {
        dlog(LOG_WARNING, "Failed to read from client connection");
        goto exit;
    }

    // Perform the command.
    if (strncmp(rdbuf, "print", SERVER_BUFLEN) == 0)
    {
        snprintf(wrbuf, SERVER_BUFLEN, "%s", "hello world");
        ret = write(client_fd, wrbuf, strnlen(wrbuf, SERVER_BUFLEN));
        if (ret <= 0)
        {
            dlog(LOG_WARNING, "Failed to write to client connection");
            goto exit;
        }
    }
    else
    {
        rdbuf[SERVER_BUFLEN-1] = '\0';
        dlog(LOG_WARNING, "Unknown command from client: %s", rdbuf);
    }

exit:
    // Done with the connection, close it.
    close(client_fd);
}

/**
 * \brief   Waits and processes incoming connections to the server until an
 *      inactivity timeout has been reached, at which point it exits.
 *
 * \param server_fd File descriptor of the server to accept from.
 */
void process_connections(int server_fd)
{
    int ret, client_fd;
    threadpool pool;
    struct pollfd server_poll;
    server_poll.fd = server_fd;
    server_poll.events = POLLIN;

    if (threadpool_create(&pool, SERVER_THREADS) < 0)
    {
        dlog(LOG_ERROR, "Failed to create threadpool");
        return;
    }

    for (;;)
    {
        // Wait for a connection with a timeout.
        ret = poll(&server_poll, 1, SERVER_TIMEOUT);
        if (ret <= 0)
        {
            // Timed out, are there active threads?
            ret = threadpool_active_threads(&pool);
            if (ret < 0)
            {
                dlog(LOG_WARNING, "Failed to count threadpool active threads");
            }
            else if (ret == 0)
            {
                dlog(LOG_INFO, "Daemon activity timeout reached");
                break;
            }
            else
            {
                dlog(LOG_INFO, "No new connections but %d active threads", ret);
            }
            continue;
        }

        // Connection must be ready, accept it.
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            dlog(LOG_ERROR, "Failed to accept client connection");
            continue;
        }

        // Spawn a thread to process the connection. The spawned thread is
        // responsible for closing the client_fd.
        dlog(LOG_INFO, "Accepted client connection, spawning handler thread");
        ret = threadpool_dispatch(&pool, process_connection, (void*)((long) client_fd));
    }

    dlog(LOG_INFO, "Processing finished, exiting");
    threadpool_destroy(&pool);
}

/**
 * \brief   Main.
 */
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
    process_connections(listen_fd);

    // Daemon finished, release lock and return.
    release_flock(&daemon_lock);
    return 0;
}
