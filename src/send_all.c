/**
 * @file send_all.c
 * @brief Implementation of the `send_all()` function declared in `send_all.h`.
 *
 * This module provides a robust way to ensure that all data is transmitted
 * over a connected TCP socket, handling partial sends gracefully.
 */

#include "send_all.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

/**
 * @brief Sends the entire contents of a buffer over a TCP socket.
 *
 * Loops calling `send()` until all data is transmitted or an error occurs.
 * Validates input parameters before proceeding.
 *
 * @param sockfd Connected TCP socket descriptor.
 * @param buf    Buffer containing data to send.
 * @param len    Number of bytes to send.
 * @return       0 on success, -1 on failure.
 */
int send_all(int sockfd, const void *buf, unsigned len) {
    // Validate input: NULL buffer with non-zero length is invalid
    if ((void*)0 == buf && len != 0) {
        printf("send_all: invalid argument (NULL buffer with non-zero length)\n");
        return -1;
    }

    // Sending zero bytes is trivially successful
    if (len == 0) {
        return 0;
    }

    const char *ptr = (const char *)buf;  // Cast to byte pointer for arithmetic
    unsigned sent = 0;                    // Number of bytes successfully sent so far

    // Loop until all data has been sent
    while (sent < len) {
        // Attempt to send remaining bytes
        ssize_t n = send(sockfd, ptr + sent, len - sent, 0);

        // Check for send failure (e.g., connection reset, signal interruption)
        if (n <= 0) {
            perror("send_all: send failed");
            return -1;
        }

        // Update count of sent bytes
        sent += (unsigned)n;
    }

    // All data sent successfully
    return 0;
}