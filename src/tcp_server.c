/**
 * @file tcp_server.c
 * @brief A multi-threaded TCP server that forwards all received data to a fixed UDP endpoint.
 *
 * This program acts as a bridge:
 *   - Listens for incoming TCP connections on a specified port.
 *   - For each client, spawns a worker thread.
 *   - Each thread reads data from the TCP client and forwards it unchanged to a preconfigured UDP server.
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

#define BUFFER_SIZE 4096  ///< Size of the per-client receive buffer

// Global UDP forwarding destination (set once at startup)
static int udp_socket;
static struct sockaddr_in udp_addr;

/**
 * @brief Worker thread function: handles data from one TCP client.
 *
 * Reads data in a loop from the connected TCP socket.
 * Immediately forwards each received chunk to the global UDP destination.
 * Exits when the client disconnects or an error occurs.
 *
 * @param arg Pointer to malloc'd int containing the client socket fd.
 * @return NULL (thread exit value unused).
 */
void* client_thread(void* arg) {
    int client_fd = *(int*)arg;
    free(arg); // Free memory allocated in main()

    char buffer[BUFFER_SIZE];

    // Continuously read from TCP client
    while (1) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);

        // Connection closed or error
        if (n <= 0) {
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
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
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
        fprintf(stderr, "Usage: %s <udp_port> <log_file>\n", argv[0]);
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

    // === Step 3: Accept clients in a loop ===
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        // Allocate memory to pass client_fd to thread (avoids race condition)
        int* client_fd = malloc(sizeof(int));
        if (!client_fd) {
            perror("malloc");
            continue;
        }

        *client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        // Spawn a new thread to handle this client
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_fd) != 0) {
            // Thread creation failed: clean up
            close(*client_fd);
            free(client_fd);
            fprintf(stderr, "Failed to create client thread\n");
            continue;
        }

        // Detach thread so its resources are auto-reclaimed on exit
        pthread_detach(tid);
    }

    // Unreachable due to infinite loop, but good practice
    close(listen_fd);
    close(udp_socket);
    return 0;
}