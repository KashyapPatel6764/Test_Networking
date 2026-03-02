// client.cpp - Sprint 2: Header Generation and Parsing
// CSC 4200 - Program 1 (LED Control)
//
// Behaviour:
// - Connects to SERVER_IP:PORT
// - Constructs a 12-byte header (Version=17, Type=1, Length=N)
// - Uses htonl() to convert header fields to network byte order
// - Sends the header, then sends the payload string
// - Receives the echoed generic packet and validates it against the original payload
// - Closes the socket cleanly

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
#define SERVER_IP "10.128.0.2"
#define PORT 9999
#define BUFFER_SIZE 256

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

// TCP may not receive everything at once, loop until all bytes are received.
static int recv_all(int fd, char *buf, size_t len)
{
    size_t total_recv = 0;

    while (total_recv < len) {
        ssize_t n = recv(fd, buf + total_recv, len - total_recv, 0);
        if (n < 0) {
            perror("[Client] recv() failed");
            return -1;
        }
        if (n == 0) {
            // Connection closed by server
            return 0;
        }
        total_recv += static_cast<size_t>(n);
    }
    return total_recv; // Success, returned total bytes read
}

int main()
{
    // 1. Create local Socket (IPv4, TCP)
    int local_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (local_sock < 0) {
        perror("[Client] socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configure server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // Initialize to zero
    
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(PORT); // Convert port to network byte order

    // Converts dotted-decimal IP string to binary representation
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("[Client] inet_pton() failed - invalid address or format");
        close(local_sock);
        exit(EXIT_FAILURE);
    }

    // 3. Connect to the server
    if (connect(local_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Client] connection to server failed");
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    std::cout << "[Client] Successfully connected to " << SERVER_IP << ":" << PORT << std::endl;

    // 4. Construct the header and payload
    std::string payload = "Hello from Client!";
    uint32_t payload_len = static_cast<uint32_t>(payload.length());
    
    // Convert header fields to network byte order using htonl()
    uint32_t net_version = htonl(VERSION);
    uint32_t net_type = htonl(TYPE_ECHO);
    uint32_t net_length = htonl(payload_len);

    char header_buffer[HEADER_SIZE];
    
    // Copy each field into the header buffer sequentially
    memcpy(header_buffer, &net_version, 4);
    memcpy(header_buffer + 4, &net_type, 4);
    memcpy(header_buffer + 8, &net_length, 4);

    // Send the 12-byte header
    if (send_all(local_sock, header_buffer, HEADER_SIZE) < 0) {
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    
    // Send the payload
    if (send_all(local_sock, payload.c_str(), payload_len) < 0) {
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    std::cout << "[Client] Sent header and message (" << payload_len << " bytes payload)." << std::endl;

    // 5. Receive the server's response
    // First, strictly receive the 12-byte header response
    char recv_header[HEADER_SIZE];
    int header_bytes = recv_all(local_sock, recv_header, HEADER_SIZE);

    if (header_bytes < 0) {
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    if (header_bytes == 0) {
        std::cout << "[Client] Server closed connection without response." << std::endl;
        close(local_sock);
        return 0;
    }

    // Extract fields back from the received header buffer
    uint32_t recv_net_version, recv_net_type, recv_net_length;
    memcpy(&recv_net_version, recv_header, 4);
    memcpy(&recv_net_type, recv_header + 4, 4);
    memcpy(&recv_net_length, recv_header + 8, 4);

    // Convert from network byte order back to host byte order using ntohl()
    uint32_t recv_version = ntohl(recv_net_version);
    uint32_t recv_type = ntohl(recv_net_type);
    uint32_t recv_length = ntohl(recv_net_length);

    std::cout << "[Client] Received Header - Version: " << recv_version 
              << ", Type: " << recv_type 
              << ", Length: " << recv_length << std::endl;

    // Receive the exactly measured payload
    char* recv_payload = new char[recv_length + 1];
    memset(recv_payload, 0, recv_length + 1);

    int payload_bytes = recv_all(local_sock, recv_payload, recv_length);
    if (payload_bytes < 0) {
        delete[] recv_payload;
        close(local_sock);
        exit(EXIT_FAILURE);
    }

    // Print the received string
    std::string received_msg(recv_payload, recv_length);
    std::cout << "[Client] Received Payload: " << received_msg << std::endl;

    // Check if the received message matches our original payload
    if (received_msg == payload) {
        std::cout << "[Client] Verification SUCCESS: Sent payload matches received payload." << std::endl;
    } else {
        std::cout << "[Client] Verification FAILED: Sent payload does not match received payload." << std::endl;
    }

    delete[] recv_payload;

    // 6. Close socket cleanly
    close(local_sock);
    std::cout << "[Client] Socket closed. Client exiting." << std::endl;

    return 0; // Success
}
