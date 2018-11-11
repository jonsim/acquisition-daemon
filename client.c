/**
 * \file   client.c
 * \author Jonathan Simmonds
 * \brief  Basic sketch of a client implementation which makes use of the
 *      acquisition daemon.
 */
#include <netinet/in.h> // sockaddr_in
#include <stdio.h>      // popen, pclose, fgets, sscanf
#include <sys/socket.h> // socket, connect
#include <unistd.h>     // read, write, close

#include "log.h" // DIE

#define RD_BUFLEN 256

static unsigned int daemon_port = 0;

/**
 * \brief   Invokes the acquisition daemon and retrieves the port for
 *      communicating with it on.
 *      NB: This uses popen to actually run the acquired binary to initiate
 *      communications with the daemon, though in a production environment
 *      execvp or similar would be more appropriate.
 *
 * \return Port number the daemon is using.
 */
unsigned int get_daemon_port(void)
{
    unsigned int port;
    FILE* proc_f = popen("./acquired", "r");
    if (!proc_f) DIE("Failed to popen daemon");

    if (!fscanf(proc_f, "%u", &port)) DIE("Failed to read from daemon");

    pclose(proc_f);
    return port;
}

/**
 * \brief   Invokes the acquisition daemon to retrieve the connection
 *      information, connects and uses the simple command interface to issue a
 *      query and print the result.
 */
void invoke_acquired(void)
{
    int socket_fd;
    struct sockaddr_in socket_addr;
    char buf[RD_BUFLEN];
    size_t read_len;

    // Get the daemon port if it has not already been acquired.
    if (daemon_port == 0)
        daemon_port = get_daemon_port();

    // Connect to the daemon.
    // TODO: AF_LOCAL?
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) DIE("Failed to create socket");

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socket_addr.sin_port = htons(daemon_port);
    if (connect(socket_fd, (struct sockaddr*) &socket_addr, sizeof(socket_addr)) < 0)
        DIE("Failed to connect to daemon at localhost:%u", daemon_port);

    // Send the command.
    if (write(socket_fd, "print", 6) <= 0) DIE("Failed to write to daemon");

    // Read the response.
    read_len = read(socket_fd, buf, RD_BUFLEN);
    if (read_len <= 0) DIE("Failed to read from daemon");
    buf[read_len] = '\0';
    printf("Successfully read from daemon: %s\n", buf);

    close(socket_fd);
}

/**
 * \brief   Main.
 */
int main(int argc, char* const argv[])
{
    invoke_acquired();

    return 0;
}