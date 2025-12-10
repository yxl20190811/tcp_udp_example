/**
 * @file tcp_server.c
 * @brief A multi-threaded TCP server that forwards all received data to a fixed UDP endpoint.
 *
 * This program acts as a bridge:
 *   - Listens for incoming TCP connections on a specified port.
 *   - For each client, spawns a worker thread.
 *   - Each thread reads data from the TCP client and forwards it unchanged to a preconfigured UDP server.
 *   - Supports graceful shutdown by typing 'quit' in the console.
 *
 * Useful for scenarios where legacy TCP clients need to send data to a UDP-only logging service.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#define BUFFER_SIZE 4096  ///< Size of the per-client receive buffer

// Global variables for thread communication
static volatile int running = 1;  ///< Flag to control server shutdown
static int listen_fd = -1;  ///< Listening socket file descriptor

// Global UDP forwarding destination (set once at startup)
static int udp_socket;
static struct sockaddr_in udp_addr;

// Structure to pass data to the client thread
typedef struct {
    int client_fd;
} client_thread_data_t;

/**
 * @brief Worker thread function: handles data from one TCP client.
 *
 * Reads data in a loop from the connected TCP socket.
 * Immediately forwards each received chunk to the global UDP destination.
 * Exits when the client disconnects, an error occurs, or shutdown is requested.
 *
 * @param arg Pointer to malloc'd struct containing the client socket fd.
 * @return NULL (thread exit value unused).
 */
void* client_thread(void* arg);

/**
 * @brief Accept thread function: handles accepting new TCP connections.
 *
 * Accepts new client connections in a loop.
 * Spawns a worker thread for each connected client.
 * Exits when shutdown is requested.
 *
 * @param arg Unused.
 * @return NULL (thread exit value unused).
 */
void* accept_thread_func(void* arg) {
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        // Set timeout on accept to allow checking running flag periodically
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
            perror("setsockopt failed in accept thread");
        }

        // Allocate memory to pass client_fd to thread (avoids race condition)
        client_thread_data_t* client_data = malloc(sizeof(client_thread_data_t));
        if (!client_data) {
            perror("malloc");
            continue;
        }

        client_data->client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);

        // Check for timeout specifically (would return -1 with errno = EAGAIN/EWOULDBLOCK)
        if (client_data->client_fd < 0) {
            // errno == EAGAIN/EWOULDBLOCK indicates timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                free(client_data);  // Free the malloc'd memory
                continue; // Go back to check running flag
            }
            // Only print error if we're still running to avoid error on shutdown
            if (running) {
                perror("accept");
            }
            free(client_data);
            continue;
        }

        // Spawn a new thread to handle this client
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_data) != 0) {
            // Thread creation failed: clean up
            close(client_data->client_fd);
            free(client_data);
            fprintf(stderr, "Failed to create client thread\n");
            continue;
        }

        // Detach thread so its resources are auto-reclaimed on exit
        pthread_detach(tid);
    }
    return NULL;
}

/**
 * @brief Worker thread function: handles data from one TCP client.
 *
 * Reads data in a loop from the connected TCP socket.
 * Immediately forwards each received chunk to the global UDP destination.
 * Exits when the client disconnects, an error occurs, or shutdown is requested.
 *
 * @param arg Pointer to malloc'd struct containing the client socket fd.
 * @return NULL (thread exit value unused).
 */
void* client_thread(void* arg) {
    client_thread_data_t* data = (client_thread_data_t*)arg;
    int client_fd = data->client_fd;
    free(data); // Free the malloc'd memory

    char buffer[BUFFER_SIZE];

    // Set socket timeout to periodically check the running flag
    struct timeval tv;
    tv.tv_sec = 1;  // 1 second timeout
    tv.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        perror("setsockopt failed in client thread");
    }

    // Continuously read from TCP client
    while (running) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);

        // Check for timeout specifically (would return -1 with errno = EAGAIN/EWOULDBLOCK)
        if (n < 0) {
            // errno == EAGAIN/EWOULDBLOCK indicates timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Go back to check running flag
            }
            // For other errors, exit the loop
            break;
        }

        // Connection closed by client
        if (n == 0) {
            break;
        }

        // Forward the exact received bytes to the UDP server
        if (sendto(udp_socket, buffer, n, 0,
                   (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
            perror("sendto (UDP forward)");
            // Continue anyway; don't break TCP connection due to UDP issue
        }
    }

    // Clean up client socket
    close(client_fd);
    return NULL;
}

/**
 * @brief Main function: sets up UDP target, starts TCP listener, accepts clients.
 *
 * Usage: ./tcp_server <tcp_listen_port> <udp_target_host> <udp_target_port>
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

    // === Step 2: Create and configure TCP listening socket ===
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("TCP socket");
        close(udp_socket);
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (serv_addr.sin_port == 0) {
        fprintf(stderr, "Usage: %s <tcp_port> <udp_host> <udp_port>\n", argv[0]);
        close(udp_socket);
        close(listen_fd);
        return 1;
    }

    // Allow reuse of local address (prevents "Address already in use" on restart)
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(udp_socket);
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        close(udp_socket);
        close(listen_fd);
        return 1;
    }

    printf("TCP server listening on port %s, forwarding to UDP %s:%s\n",
           argv[1], argv[2], argv[3]);
    printf("Type 'quit' and press Enter to exit the server gracefully.\n");

    // === Step 3: Start accept thread ===
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, accept_thread_func, NULL) != 0) {
        perror("pthread_create for accept thread");
        close(udp_socket);
        close(listen_fd);
        return 1;
    }

    // === Step 4: Main thread waits for user input to quit ===
    char input[10];
    while (running) {
        if (fgets(input, sizeof(input), stdin)) {
            if (strncmp(input, "quit", 4) == 0) {
                // Set running flag to 0 to signal other threads to stop
                running = 0;
                printf("Shutting down TCP server...\n");
                break;
            }
        }
    }

    // Wait for the accept thread to finish
    pthread_join(accept_thread, NULL);

    // Close listening socket to unblock any pending accept calls
    if (listen_fd >= 0) {
        close(listen_fd);
    }

    // Close UDP socket
    close(udp_socket);

    printf("TCP server stopped.\n");
    return 0;
}