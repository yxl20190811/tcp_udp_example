/**
 * @file epoll_server.c
 * @brief An epoll-based TCP server that forwards all received data to a fixed UDP endpoint.
 *
 * This program acts as a bridge:
 *   - Uses epoll to efficiently handle multiple TCP connections.
 *   - Each connected TCP client's data is forwarded to a preconfigured UDP server.
 *   - Supports graceful shutdown by typing 'quit' in the console.
 *
 * This implementation is more efficient than the multi-threaded approach for handling
 * many concurrent connections, as it uses a single-threaded event loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#define BUFFER_SIZE 4096  ///< Size of the per-client receive buffer
#define MAX_EVENTS 64     ///< Maximum number of events to return from epoll_wait

// Global variables for epoll and sockets
static int running = 1;      ///< Flag to control server shutdown
static int listen_fd = -1;   ///< Listening socket file descriptor
static int epoll_fd = -1;    ///< Epoll file descriptor

// Global UDP forwarding destination (set once at startup)
static int udp_socket;
static struct sockaddr_in udp_addr;

/**
 * @brief Set a socket to non-blocking mode.
 *
 * @param fd The file descriptor to set to non-blocking mode.
 * @return 0 on success, -1 on error.
 */
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

/**
 * @brief Adds a file descriptor to the epoll instance.
 *
 * @param fd The file descriptor to add.
 * @return 0 on success, -1 on error.
 */
int add_to_epoll(int fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Level-triggered read events
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl: add fd");
        return -1;
    }
    return 0;
}

/**
 * @brief Removes a file descriptor from the epoll instance.
 *
 * @param fd The file descriptor to remove.
 * @return 0 on success, -1 on error.
 */
int remove_from_epoll(int fd) {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        perror("epoll_ctl: remove fd");
        return -1;
    }
    return 0;
}

/**
 * @brief Handles incoming data from a TCP client.
 *
 * Reads data from the client socket and forwards it to the UDP destination.
 * If an error occurs or the client disconnects, cleans up the connection.
 *
 * @param client_fd The client socket file descriptor.
 * @return 0 on success, -1 on error.
 */
int handle_client_data(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // We've read all available data
                break;
            } else {
                // Actual error occurred
                perror("recv from client");
                return -1;
            }
        } else if (bytes_read == 0) {
            // Client disconnected
            printf("Client disconnected (fd: %d)\n", client_fd);
            return -1;
        }

        // Forward the exact received bytes to the UDP server
        if (sendto(udp_socket, buffer, bytes_read, 0,
                   (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
            perror("sendto (UDP forward)");
            // Continue anyway; don't break TCP connection due to UDP issue
        }
    }
    return 0;
}

/**
 * @brief Main function: sets up UDP target, starts TCP listener using epoll, handles clients.
 *
 * Usage: ./epoll_server <tcp_listen_port> <udp_target_host> <udp_target_port>
 *
 * @param argc Must be 4.
 * @param argv [prog, tcp_port, udp_host, udp_port]
 * @return Exit status.
 */
int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <tcp_port> <udp_host> <udp_port>\n", argv[0]);
        return 1;
    }

    // === Step 1: Set up UDP forwarding socket ===
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("UDP socket");
        return 1;
    }

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(atoi(argv[3]));
    if (inet_pton(AF_INET, argv[2], &udp_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid UDP host\n");
        close(udp_socket);
        return 1;
    }

    // === Step 2: Create epoll instance ===
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(udp_socket);
        return 1;
    }

    // === Step 3: Create and configure TCP listening socket ===
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("TCP socket");
        close(udp_socket);
        close(epoll_fd);
        return 1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(udp_socket);
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (serv_addr.sin_port == 0) {
        fprintf(stderr, "Usage: %s <tcp_port> <udp_host> <udp_port>\n", argv[0]);
        close(udp_socket);
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(udp_socket);
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        close(udp_socket);
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    // Set listening socket to non-blocking mode
    if (set_nonblocking(listen_fd) == -1) {
        close(udp_socket);
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    // Add listening socket to epoll
    if (add_to_epoll(listen_fd) == -1) {
        close(udp_socket);
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    printf("Epoll-based TCP server listening on port %s, forwarding to UDP %s:%s\n",
           argv[1], argv[2], argv[3]);
    printf("Type 'quit' and press Enter to exit the server gracefully.\n");

    // === Step 4: Main event loop ===
    struct epoll_event events[MAX_EVENTS];
    char input[10];  // For reading user input

    // Set up stdin for epoll in case we want to handle it asynchronously
    fd_set stdin_set;
    FD_ZERO(&stdin_set);
    FD_SET(STDIN_FILENO, &stdin_set);

    while (running) {
        // Check for user input without blocking the entire server
        FD_ZERO(&stdin_set);
        FD_SET(STDIN_FILENO, &stdin_set);

        struct timeval tv = {0, 0}; // No timeout - just check if data is available
        if (select(STDIN_FILENO + 1, &stdin_set, NULL, NULL, &tv) > 0) {
            if (FD_ISSET(STDIN_FILENO, &stdin_set)) {
                if (fgets(input, sizeof(input), stdin)) {
                    if (strncmp(input, "quit", 4) == 0) {
                        running = 0;
                        printf("Shutting down epoll TCP server...\n");
                        break;
                    }
                }
            }
        }

        // Wait for events from epoll
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 100); // 100ms timeout
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;  // Signal interrupted, continue loop
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                // New connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                // Set client socket to non-blocking mode
                if (set_nonblocking(client_fd) == -1) {
                    close(client_fd);
                    continue;
                }

                // Add client socket to epoll
                if (add_to_epoll(client_fd) == -1) {
                    close(client_fd);
                    continue;
                }

                printf("New client connected (fd: %d)\n", client_fd);
            } else {
                // Data from existing client
                int client_fd = events[i].data.fd;
                if (handle_client_data(client_fd) == -1) {
                    // Client disconnected or error occurred, remove from epoll and close
                    remove_from_epoll(client_fd);
                    close(client_fd);
                }
            }
        }
    }

    // Cleanup
    close(listen_fd);
    close(udp_socket);
    close(epoll_fd);

    printf("Epoll-based TCP server stopped.\n");
    return 0;
}