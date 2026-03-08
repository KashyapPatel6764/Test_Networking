// server.cpp - Sprint 3: Header Generation, Parsing, and Float Payload
// CSC 4200 - Program 1 (LED Control)
//
// Behaviour:
// - Binds to PORT and listens for clients
// - Enters a loop handling new connections without exiting
// - Receives precisely a 12-byte header
// - Decodes fields, ensures VERSION=17 using ntohl()
// - Dispatches based on Message Type:
//     Type 1 (ECHO)  : reads N-byte string payload, prints and echoes it
//     Type 2 (FLOAT) : reads 4-byte float payload, prints and echoes it
// - Re-encodes the header identically using htonl() and echoes the full packet back

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

#include "protocol.h"

// Configuration
#define PORT 9999
#define BACKLOG 5       // max pending connections in listen() queue

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

// TCP may not receive everything at once, loop until all bytes are received.
static int recv_all(int fd, char *buf, size_t len)
{
    size_t total_recv = 0;

    while (total_recv < len) {
        ssize_t n = recv(fd, buf + total_recv, len - total_recv, 0);
        if (n < 0) {
            perror("[Server] recv() failed");
            return -1;
        }
        if (n == 0) {
            // Client cleanly closed the connection before full buffer read
            return 0;
        }
        total_recv += static_cast<size_t>(n);
    }
    return total_recv; // Success, returned total bytes read
}

// Handle one connected client through full message sequence
static void handle_client(int client_fd, struct sockaddr_in *client_addr)
{
    // Convert client IP address to a readable string format
    std::string client_ip = inet_ntoa(client_addr->sin_addr);
    int client_port = ntohs(client_addr->sin_port);

    // Print who connected
    std::cout << "[Server] Client connected: " << client_ip << ":" << client_port << std::endl;

    while (true) {
        // 1. Receive 12-byte header explicitly
        char recv_header[HEADER_SIZE];
        int header_bytes = recv_all(client_fd, recv_header, HEADER_SIZE);

        if (header_bytes < 0) {
            perror("[Server] recv_all() failed");
            break; // Exit the receive loop for this client
        }
        if (header_bytes == 0) {
             std::cout << "[Server] Client disconnected normally." << std::endl;
             break; // Exit the receive loop for this client
        }
        // Enforce strong framing: it should be 12 bytes.
        if (static_cast<size_t>(header_bytes) != HEADER_SIZE) {
            std::cerr << "[Server] Received incorrectly sized header chunk." << std::endl;
            break;
        }

        // Extract fields using memcpy to safely interpret memory back to integers
        uint32_t net_version, net_type, net_length;
        memcpy(&net_version, recv_header, 4);
        memcpy(&net_type, recv_header + 4, 4);
        memcpy(&net_length, recv_header + 8, 4);

        // Convert the header arguments back from big-endian format to integers
        uint32_t recv_version = ntohl(net_version);
        uint32_t recv_type    = ntohl(net_type);
        uint32_t recv_length  = ntohl(net_length);

        std::cout << "[Server] Received Header - Version: " << recv_version 
                  << ", Type: " << recv_type 
                  << ", Length: " << recv_length << std::endl;

        // Verify Protocol Version
        if (recv_version != VERSION) {
            std::cerr << "[Server] Version Mismatch! Expected " << VERSION << ", got " << recv_version << std::endl;
            break; // Disconnect the bad client
        }

        // 2. Receive the payload (exactly recv_length bytes)
        char* recv_payload = new char[recv_length + 1];
        memset(recv_payload, 0, recv_length + 1);

        int payload_bytes = recv_all(client_fd, recv_payload, recv_length);
        if (payload_bytes < 0) {
            perror("[Server] Error receiving payload buffer from client.");
            delete[] recv_payload;
            break;
        }
        if (payload_bytes == 0) {
            delete[] recv_payload;
            break;
        }

        // 3. Dispatch based on Message Type
        if (recv_type == TYPE_ECHO) {
            // ---- Sprint 2: String echo ----
            std::cout << "[Server] Type 1 (String Echo) - Payload (" << payload_bytes 
                      << " bytes): " << recv_payload << std::endl;

        } else if (recv_type == TYPE_FLOAT) {
            // ---- Sprint 3: Float echo ----
            // Interpret the 4-byte payload as a float using memcpy (safe, no aliasing)
            if (recv_length != sizeof(float)) {
                std::cerr << "[Server] Type 2 (Float) expects 4-byte payload, got " 
                          << recv_length << " bytes." << std::endl;
                delete[] recv_payload;
                break;
            }
            float recv_float;
            memcpy(&recv_float, recv_payload, sizeof(float));
            std::cout << "[Server] Type 2 (Float Echo) - Received float value: " 
                      << recv_float << std::endl;

        } else {
            std::cerr << "[Server] Unknown message type: " << recv_type << std::endl;
            delete[] recv_payload;
            break;
        }

        // 4. Echo back: repackage the exact same header + payload
        char send_header[HEADER_SIZE];
        memcpy(send_header, &net_version, 4);
        memcpy(send_header + 4, &net_type, 4);
        memcpy(send_header + 8, &net_length, 4);

        // Send exactly 12 bytes of Header back
        if (send_all(client_fd, send_header, HEADER_SIZE) < 0) {
            delete[] recv_payload;
            break;
        }
        
        // Send exactly `N` payload bytes back
        if (send_all(client_fd, recv_payload, recv_length) < 0) {
            delete[] recv_payload;
            break;
        }
        
        std::cout << "[Server] Sent echoed packet response to client." << std::endl;
        delete[] recv_payload;
    }

    // Always close the client socket when done
    close(client_fd);
    std::cout << "[Server] Client socket closed." << std::endl;
    std::cout << "[Server] Waiting for next connection...\n" << std::endl;
}

int main()
{
    // 1. Create TCP socket
    // AF_INET: IPv4, SOCK_STREAM: TCP, 0: Default protocol
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Server] socket() creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure socket options:
    // SO_REUSEADDR lets us immediately rebind to the port after the process exits
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[Server] setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. Configure server address and bind
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // Initialize to zeros
    
    server_addr.sin_family      = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any local IP
    server_addr.sin_port        = htons(PORT); // Convert port to network byte order

    // Bind the socket to the specified address and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Server] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for incoming connections
    // Marks the socket as passive listening
    if (listen(server_fd, BACKLOG) < 0) {
        perror("[Server] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] Listening on port " << PORT << "...\n" << std::endl;

    // 4. Accept loop
    // The server must NOT exit after one client.
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Block until a client connects. Returns a new file descriptor for the client.
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            perror("[Server] accept() failed - continuing to wait");
            continue;
        }

        // Process the client connection
        handle_client(client_fd, &client_addr);
    }

    // Unreachable code, but good practice to include
    close(server_fd);
    return 0;
}
