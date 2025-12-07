/**
 * @file udp_server.c
 * @brief A simple UDP server that listens on a specified port and appends all received messages to a log file.
 *
 * This program binds to a UDP port, receives datagrams from any client,
 * and writes them verbatim to a specified log file in append mode.
 * It runs indefinitely until terminated externally.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 4096  ///< Maximum size of a UDP datagram we can receive

/**
 * @brief Main entry point for the UDP logging server.
 *
 * Usage: ./udp_server <udp_port> <log_file>
 *
 * The server:
 *   - Creates a UDP socket.
 *   - Binds it to INADDR_ANY on the given port.
 *   - Opens the log file in append mode with buffering disabled.
 *   - Enters an infinite loop receiving datagrams and writing them to the file.
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

    // Receive loop
    char buffer[BUFFER_SIZE];
    while (1) {
        // Receive a datagram (ignore sender address since we don't need it)
        ssize_t n = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            continue; // Continue on transient errors
        }

        // Null-terminate for safety (though not strictly needed for binary-safe logs)
        buffer[n] = '\0';

        // Write the received message to the log file
        fprintf(fp, "%s", buffer);
        fflush(fp); // Ensure data is written immediately
    }

    // Note: The following lines are unreachable due to infinite loop,
    // but included for completeness and static analysis tools.
    fclose(fp);
    close(sock_fd);
    return 0;
}