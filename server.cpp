/*
 * server.cpp  –  Sprint 1: Basic TCP Server (C++ Version)
 *
 * CSC 4200  –  Program 1 (LED Control)
 *
 * Behaviour:
 *   - Binds to PORT and listens for clients.
 *   - For each connection: receives all data, prints it,
 *     sends an "ACK: <message>" reply, then closes the
 *     client socket.
 *   - Loops forever so it keeps running after every disconnect.
 *
 * Compile:  g++ -Wall -Wextra -g -o server server.cpp
 * Run:      ./server
 */

#include <iostream>
#include <string>
#include <vector>
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
#define PORT        9999
#define BACKLOG     5       /* max pending connections in listen() queue   */
#define BUFFER_SIZE 1024

/* ─── Helper: send all bytes (handles partial sends) ────────────────────── */
// TCP does not guarantee that 'send()' will transmit all requested bytes
// in a single call. We must loop until all bytes are successfully sent.
static int send_all(int fd, const char *buf, size_t len)
{
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t n = send(fd, buf + total_sent, len - total_sent, 0);
        if (n < 0) {
            perror("[Server] send() failed");
            return -1;
        }
        total_sent += static_cast<size_t>(n);
    }
    return 0;
}

/* ─── Handle one connected client ──────────────────────────────────────── */
static void handle_client(int client_fd, struct sockaddr_in *client_addr)
{
    char buffer[BUFFER_SIZE];
    
    // Convert client IP address to a readable string format
    std::string client_ip = inet_ntoa(client_addr->sin_addr);
    int client_port = ntohs(client_addr->sin_port);

    /* Print who connected */
    std::cout << "[Server] Client connected: " << client_ip << ":" << client_port << std::endl;

    /*
     * recv() loop
     *
     * TCP is a byte stream. recv() may return fewer bytes than the
     * buffer can hold. We call it in a loop until the client closes
     * the connection (recv returns 0) or an error occurs.
     *
     * For Sprint 1 we echo-respond after every chunk we receive.
     */
    while (true) {
        memset(buffer, 0, sizeof(buffer));

        // Receive data from the client
        ssize_t bytes_recv = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_recv < 0) {
            /* Real error occurred during receive */
            perror("[Server] recv() failed");
            break;  // Exit the receive loop for this client
        }

        if (bytes_recv == 0) {
            /*
             * recv() returning 0 means the client closed the connection
             * (sent TCP FIN). This is the clean shutdown signal.
             */
            std::cout << "[Server] Client disconnected (recv returned 0)." << std::endl;
            break;  // Exit the receive loop for this client
        }

        /* bytes_recv > 0: we successfully got data */
        buffer[bytes_recv] = '\0';   /* Null-terminate the string for safe printing */
        std::cout << "[Server] Received (" << bytes_recv << " bytes): " << buffer << std::endl;

        /* Build response message "ACK: <message>" */
        std::string response = "ACK: ";
        response += buffer;

        // Send the complete response back to the client
        if (send_all(client_fd, response.c_str(), response.length()) < 0) {
            /* Error already printed inside send_all */
            break;  // Exit on send failure
        }
        std::cout << "[Server] Sent response: " << response << std::endl;
    }

    // Always close the client socket when done
    close(client_fd);
    std::cout << "[Server] Client socket closed." << std::endl;
    std::cout << "[Server] Waiting for next connection...\n" << std::endl;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main()
{
    /* ── 1. Create TCP socket ── */
    // AF_INET: IPv4, SOCK_STREAM: TCP, 0: Default protocol
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Server] socket() creation failed");
        exit(EXIT_FAILURE);
    }

    /*
     * Configure socket options:
     * SO_REUSEADDR lets us immediately rebind to the port after the process exits
     * without waiting for the OS TIME_WAIT state to expire. Useful during testing.
     */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[Server] setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* ── 2. Configure server address and bind ── */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); /* Initialize to zeros */
    
    server_addr.sin_family      = AF_INET; /* IPv4 */
    server_addr.sin_addr.s_addr = INADDR_ANY; /* Accept connections on any local IP (0.0.0.0) */
    server_addr.sin_port        = htons(PORT); /* Convert port to network byte order */

    // Bind the socket to the specified address and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Server] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* ── 3. Listen for incoming connections ── */
    // Marks the socket as a passive socket that will be used to accept incoming
    // connection requests using accept.
    if (listen(server_fd, BACKLOG) < 0) {
        perror("[Server] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] Listening on port " << PORT << "...\n" << std::endl;

    /*
     * ── 4. Accept loop ──
     *
     * The server must NOT exit after one client.
     * We loop forever, blocking on accept() until a new client connection arrives.
     */
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Block until a client connects. Returns a new file descriptor for the client.
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            /*
             * accept() can fail spuriously (e.g., EINTR from a signal).
             * We print the error and continue rather than crashing the server.
             */
            perror("[Server] accept() failed – continuing to wait");
            continue;
        }

        // Process the client connection
        handle_client(client_fd, &client_addr);
    }

    /* Unreachable code, but good practice to include */
    close(server_fd);
    return 0;
}
