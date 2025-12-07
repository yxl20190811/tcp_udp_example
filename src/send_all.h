/**
 * @file send_all.h
 * @brief Declaration of a utility function to reliably send all data over a blocking TCP socket.
 *
 * The standard `send()` system call may transmit only part of the data in a single call,
 * even on blocking sockets. This header declares `send_all()`, which ensures that
 * the entire buffer is sent or an error occurs.
 */

#ifndef SEND_ALL_H
#define SEND_ALL_H

/**
 * @brief Reliably send all data through a blocking TCP socket.
 *
 * Even in blocking mode, `send()` may perform a partial write (i.e., send fewer bytes
 * than requested). This function loops until either:
 *   - All `len` bytes from `buf` are successfully sent, or
 *   - A system error occurs (e.g., connection closed by peer).
 *
 * @param sockfd  A connected TCP socket file descriptor (SOCK_STREAM type).
 * @param buf     Pointer to the data buffer to be sent. Must not be NULL if `len > 0`.
 * @param len     Number of bytes to send. If zero, the function returns success immediately.
 *
 * @return        Returns 0 on complete success; returns -1 on any failure.
 *                On error, a descriptive message is printed to stderr via `perror()`.
 *
 * @note          This function is intended ONLY for use with **blocking** TCP sockets.
 *                It does not handle non-blocking sockets or signals interrupting `send()`.
 */
int send_all(int sockfd, const void *buf, unsigned len);

#endif // SEND_ALL_H