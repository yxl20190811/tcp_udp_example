/**
 * @file udp_server.c
 * @brief A UDP server that listens on a specified port and appends all received messages to a log file.
 *
 * This program binds to a UDP port, receives datagrams from any client,
 * and writes them verbatim to a specified log file in append mode.
 * It supports graceful shutdown by typing 'quit' in the console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

#define BUFFER_SIZE 4096  ///< Maximum size of a UDP datagram we can receive

// Global variable for thread communication
static volatile int running = 1;  ///< Flag to control server shutdown

// Structure to pass data to the thread
typedef struct {
    int sock_fd;
    FILE* fp;
} thread_data_t;

/**
 * @brief Worker thread function: handles receiving UDP datagrams and writing to log file.
 *
 * @param arg Pointer to malloc'd struct containing socket fd and log file pointer.
 * @return NULL (thread exit value unused).
 */
void* udp_receive_thread(void* arg) {
    // Extract socket fd and file pointer from argument
    thread_data_t* data = (thread_data_t*)arg;
    int sock_fd = data->sock_fd;
    FILE* fp = data->fp;
    free(data); // Free the malloc'd memory

    // Set socket timeout to periodically check the running flag
    struct timeval tv;
    tv.tv_sec = 1;  // 1 second timeout
    tv.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        perror("setsockopt failed");
        return NULL;
    }

    char buffer[BUFFER_SIZE];

    while (1) {
        // Check if we should stop
        if (!running) {
            break;
        }

        // Receive a datagram (ignore sender address since we don't need it)
        ssize_t n = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);

        // Check for timeout specifically (would return -1 with errno = EAGAIN/EWOULDBLOCK)
        if (n < 0) {
            // errno == EAGAIN/EWOULDBLOCK indicates timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Go back to check running flag
            }
            perror("recvfrom");
            continue; // Continue on other transient errors
        }

        // Check running flag again before writing to file
        if (!running) {
            break;
        }

        // Null-terminate for safety (though not strictly needed for binary-safe logs)
        buffer[n] = '\0';

        // Write the received message to the log file
        fprintf(fp, "%s", buffer);
        fflush(fp); // Ensure data is written immediately
    }

    return NULL;
}

/**
 * @brief Main entry point for the UDP logging server.
 *
 * Usage: ./udp_server <udp_port> <log_file>
 *
 * The server:
 *   - Creates a UDP socket.
 *   - Binds it to INADDR_ANY on the given port.
 *   - Opens the log file in append mode with buffering disabled.
 *   - Starts a thread to receive datagrams and write them to the file.
 *   - Main thread waits for user input to shutdown gracefully.
 *
 * @param argc Argument count (must be 3).
 * @param argv Arguments: [program_name, udp_port, log_file]
 * @return Exit status (0 on normal operation, 1 on error).
 */
int main(int argc, char* argv[]) {
    // Validate command-line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <udp_port> <log_file>\n", argv[0]);
        return 1;
    }

    // Create a UDP socket (SOCK_DGRAM = connectionless datagram socket)
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    // Prepare the server address structure
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;           // IPv4
    serv_addr.sin_addr.s_addr = INADDR_ANY;   // Accept packets on any interface
    serv_addr.sin_port = htons(atoi(argv[1])); // Convert port to network byte order

    // Basic validation: ensure port is non-zero
    if (serv_addr.sin_port == 0) {
        fprintf(stderr, "Usage: %s <udp_port> <log_file>\n", argv[0]);
        close(sock_fd);
        return 1;
    }

    // Bind the socket to the specified port
    if (bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return 1;
    }

    // Open log file in append mode
    FILE* fp = fopen(argv[2], "a");
    if (!fp) {
        perror("fopen");
        close(sock_fd);
        return 1;
    }

    // Disable buffering to ensure immediate writes (important for logs)
    setbuf(fp, NULL);

    printf("UDP server listening on port %s, writing to %s\n", argv[1], argv[2]);
    printf("Type 'quit' and press Enter to exit the server gracefully.\n");

    // Prepare arguments for the thread
    thread_data_t* thread_data = malloc(sizeof(thread_data_t));
    if (!thread_data) {
        perror("malloc");
        fclose(fp);
        close(sock_fd);
        return 1;
    }
    thread_data->sock_fd = sock_fd;
    thread_data->fp = fp;

    // Start the UDP receiving thread
    pthread_t udp_thread;
    if (pthread_create(&udp_thread, NULL, udp_receive_thread, thread_data) != 0) {
        perror("pthread_create");
        fclose(fp);
        close(sock_fd);
        free(thread_data);
        return 1;
    }

    // Main thread: wait for user input to quit
    char input[10];
    while (running) {
        if (fgets(input, sizeof(input), stdin)) {
            if (strncmp(input, "quit", 4) == 0) {
                // Set running flag to 0 to signal other thread to stop
                running = 0;
                printf("Shutting down UDP server...\n");
                break;
            }
        }
    }

    // Wait for the UDP thread to finish
    pthread_join(udp_thread, NULL);

    // Close file and socket
    fclose(fp);
    close(sock_fd);

    printf("UDP server stopped.\n");
    return 0;
}