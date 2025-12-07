/**
 * @file test_client.c
 * @brief A versatile test client that can send a formatted message via TCP or UDP.
 *
 * Constructs a timestamped log message including source file and line number,
 * then sends it to a remote host using either TCP (connection-oriented) or UDP (datagram).
 *
 * Usage:
 *   ./test_client tcp <host> <port> "<message>"
 *   ./test_client udp <host> <port> "<message>"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "send_all.h"

/**
 * @brief Main function: parses arguments, formats message, and sends via TCP or UDP.
 *
 * The message format is:
 *   [YYYY-MM-DD HH:MM:SS][user_message][source_file][line_number]
 *
 * @param argc Argument count (must be ≥5).
 * @param argv Arguments: [prog, mode, host, port, message]
 * @return Exit code (0 on success, 1 on error).
 */
int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "Usage:\n"
                "  %s tcp <host> <port> <message>\n"
                "  %s udp <host> <port> <message>\n",
                argv[0], argv[0]);
        return 1;
    }

    const char* mode = argv[1];   // "tcp" or "udp"
    const char* host = argv[2];   // Destination IP or hostname
    int port = atoi(argv[3]);     // Destination port
    const char* msg = argv[4];    // User-provided message

    // Construct the full log message with timestamp, file, and line
    char buf[10000];
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    snprintf(buf, sizeof(buf),
             "[%04d-%02d-%02d %02d:%02d:%02d][%s][%s][%d]\n",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec,
             msg, __FILE__, __LINE__);

    size_t msg_len = strlen(buf);

    // Prepare destination address
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (addr.sin_port == 0) {
        fprintf(stderr, "Usage: %s <udp_port> <log_file>\n", argv[0]);
        return 1;
    }

    // Convert hostname/IP string to binary form
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid host\n");
        return 1;
    }

    if (strcmp(mode, "tcp") == 0) {
        // === TCP Mode ===
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("TCP socket");
            return 1;
        }

        // Establish connection to server
        if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            close(sock_fd);
            return 1;
        }

        // Send entire message reliably
        if (send_all(sock_fd, buf, msg_len) != 0) {
            fprintf(stderr, "Failed to send TCP message\n");
            close(sock_fd);
            return 1;
        }

        close(sock_fd);
        printf("TCP message sent to %s:%d\n", host, port);

    } else if (strcmp(mode, "udp") == 0) {
        // === UDP Mode ===
        int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            perror("UDP socket");
            return 1;
        }

        // Send datagram in one shot (no connection needed)
        if (sendto(sock_fd, buf, msg_len, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("sendto");
            close(sock_fd);
            return 1;
        }

        close(sock_fd);
        printf("UDP message sent to %s:%d\n", host, port);

    } else {
        fprintf(stderr, "Invalid mode: %s (must be 'tcp' or 'udp')\n", mode);
        return 1;
    }

    return 0;
}