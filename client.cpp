/*
 * client.cpp  –  Sprint 1: Basic TCP Client (C++ Version)
 *
 * CSC 4200  –  Program 1 (LED Control)
 *
 * Behaviour:
 *   - Connects to SERVER_IP:PORT.
 *   - Sends the message "HELLO".
 *   - Receives the server's response and prints it.
 *   - Closes the socket cleanly.
 *
 * Compile:  g++ -Wall -Wextra -g -o client client.cpp
 * Run:      ./client
 */

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ─── Configuration ────────────────────────────────────────────────────── */
#define SERVER_IP   "127.0.0.1"
#define PORT        9999
#define BUFFER_SIZE 256

/* ─── Helper: send all bytes (handles partial sends) ────────────────────── */
// TCP is a stream protocol; a single 'send' call may not send the entire buffer.
// This function loops until all bytes are reliably pushed to the OS buffers.
static int send_all(int fd, const char *buf, size_t len)
{
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t n = send(fd, buf + total_sent, len - total_sent, 0);
        if (n < 0) {
            perror("[Client] send() failed");
            return -1;
        }
        total_sent += static_cast<size_t>(n);
    }
    return 0;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main()
{
    /* ── 1. Create local Socket ── */
    // AF_INET: IPv4, SOCK_STREAM: TCP protocol
    int local_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (local_sock < 0) {
        perror("[Client] socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* ── 2. Configure server address ── */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); /* Initialize struct to zero */
    
    server_addr.sin_family = AF_INET; /* IPv4 */
    server_addr.sin_port   = htons(PORT); /* Convert port to network byte order */

    /*
     * inet_pton() converts a dotted-decimal IP string (e.g., "127.0.0.1")
     * to its binary representation in network byte order.
     */
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("[Client] inet_pton() failed – invalid address or format");
        close(local_sock);
        exit(EXIT_FAILURE);
    }

    /* ── 3. Connect to the server ── */
    // Initiate connection to the server's TCP socket
    if (connect(local_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Client] connection to server failed");
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    std::cout << "[Client] Successfully connected to " << SERVER_IP << ":" << PORT << std::endl;

    /* ── 4. Send message ── */
    std::string message = "HELLO";
    size_t msg_len = message.length();

    // Use our helper to ensure the entire message is sent
    if (send_all(local_sock, message.c_str(), msg_len) < 0) {
        /* Error already printed inside send_all */
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    std::cout << "[Client] Message sent to server (" << msg_len << " bytes): " << message << std::endl;

    /*
     * ── 5. Receive response ──
     *
     * In TCP, 'recv' might return fewer bytes than what was actually sent
     * in a single chunk. We wait to receive the server's ACK.
     */
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Wait to receive the reply from the server
    ssize_t bytes_recv = recv(local_sock, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_recv < 0) {
        // A network error occurred
        perror("[Client] recv failed");
        close(local_sock);
        exit(EXIT_FAILURE);
    }

    if (bytes_recv == 0) {
        /*
         * Server closed the connection before sending a response.
         * This usually means the server process ended prematurely.
         */
        std::cout << "[Client] Server closed connection without sending a response." << std::endl;
    } else {
        buffer[bytes_recv] = '\0'; // Null-terminate the received data for safe C-string printing
        std::cout << "[Client] Response from server (" << bytes_recv << " bytes): " << buffer << std::endl;
    }

    /* ── 6. Close socket cleanly ── */
    // Free the file descriptor and close the TCP connection
    close(local_sock);
    std::cout << "[Client] Socket closed. Client exiting." << std::endl;

    return 0; // Success
}
