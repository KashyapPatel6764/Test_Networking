// server.cpp - Sprint 3: Header Generation, Parsing, and Float Payload
// CSC 4200 - Program 1 (LED Control)

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

// SPRINT 1 EXPLANATION — Why send() may not send all bytes at once:

//   TCP is a BYTE STREAM protocol. When you call send(), the operating system
//   copies your data into a kernel send buffer. If the buffer is full or the
//   network is congested, send() may only transmit PART of your data and return
//   a count smaller than what you requested.
//
//   For example, if you call send(fd, buf, 100, 0), it might only send 60 bytes
//   and return 60. The remaining 40 bytes are YOUR responsibility to send in a
//   follow-up call.

//   This is the fundamental difference between send() and recv():
//     - send() pushes bytes INTO the network. It may accept fewer bytes than
//       you offer if the OS buffer is full. It returns how many bytes it accepted.
//     - recv() pulls bytes FROM the network. It returns however many bytes are
//       currently available, which may be fewer than you asked for.

//   Both are NON-GUARANTEED in terms of the number of bytes they handle per call.
//   That is why we must ALWAYS loop until all bytes are sent/received.

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

// SPRINT 1 EXPLANATION — Why recv() may return fewer bytes than requested:

//   TCP does NOT preserve message boundaries. It is a continuous byte stream.
//   When the sender sends 100 bytes, the receiver might get them in chunks:
//   first recv() returns 40 bytes, next returns 60 bytes. Or all 100 at once.
//   There is NO guarantee.

//   This happens because:
//     1) The OS buffers data and delivers whatever is available when recv() is called.
//     2) Network packets can be split, delayed, or combined by routers and the OS.
//     3) The sender's data may arrive across multiple TCP segments.

//   WHAT DOES recv() RETURNING 0 MEAN?
//     When recv() returns 0, it means the other side has CLEANLY CLOSED the
//     connection (called close() on their socket). It is NOT an error — it is
//     the normal way TCP signals "I'm done sending data." We must detect this
//     and stop trying to read, otherwise we'd loop forever on zero-byte reads.

//   That is why recv_all() loops: we keep calling recv() and accumulating bytes
//   until we have received EXACTLY the number of bytes we expect, or until
//   the connection closes (returns 0) or an error occurs (returns -1).

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
            // recv() returned 0 → client closed the connection gracefully.
            // This is not an error. We return 0 to signal "no more data."
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
        // SPRINT 2 EXPLANATION — Receiving the header with recv() in a loop:
        
        //   We need EXACTLY 12 bytes to form a complete header. But TCP might
        //   deliver only 4 bytes on the first recv() call, then 8 bytes on the
        //   next — or any other combination. That is why recv_all() loops until
        //   it has accumulated all 12 bytes.
        
        //   If we only called recv() once and assumed we got 12 bytes, we could
        //   read an INCOMPLETE header, causing Version/Type/Length to be wrong.
        //   This would corrupt all subsequent parsing.
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

        // SPRINT 2 EXPLANATION — Why we use ntohl() after receiving:
        
        //   Different CPU architectures store multi-byte integers differently:
        //     - Big-endian (network byte order): most significant byte first
        //     - Little-endian (e.g., x86/x64):  least significant byte first
        
        //   The number 17 (0x00000011) is stored as:
        //     Big-endian:    [00] [00] [00] [11]
        //     Little-endian: [11] [00] [00] [00]
        
        //   If a big-endian machine sends 17 and a little-endian machine reads
        //   those raw bytes without conversion, it would interpret them as
        //   0x11000000 = 285,212,672 — completely WRONG.
        
        //   The solution: ALWAYS convert to network byte order (big-endian)
        //   before sending with htonl(), and convert back to host byte order
        //   after receiving with ntohl(). This ensures both sides agree on the
        //   values regardless of their CPU architecture.
        
        //   We use memcpy() to safely extract each 4-byte field from the raw
        //   buffer into a uint32_t, then apply ntohl() to convert.
        uint32_t net_version, net_type, net_length;
        memcpy(&net_version, recv_header, 4);
        memcpy(&net_type, recv_header + 4, 4);
        memcpy(&net_length, recv_header + 8, 4);

        // Convert from network byte order (big-endian) back to host byte order
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

        // SPRINT 2 EXPLANATION — How Message Length determines framing:
        
        //   TCP is a raw byte stream with NO message boundaries. If the client
        //   sends a 12-byte header followed by a 5-byte payload, TCP just sees
        //   17 bytes in a row — it has no idea where the header ends and the
        //   payload begins.
        
        //   The HEADER provides the framing. After reading 12 bytes (the fixed
        //   header size), we extract the Message Length field. This tells us
        //   EXACTLY how many more bytes to read for the payload.
        
        //   Without Message Length, we would have no way to know:
        //     - When to stop reading the payload
        //     - Where one message ends and the next begins
        //     - Whether we've received the complete message or only part of it
        
        //   This is why header-based framing is essential in TCP protocols.
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
            // SPRINT 3 EXPLANATION — Why the float payload is exactly 4 bytes:
            
            //   A float in C/C++ follows the IEEE 754 single-precision format,
            //   which is ALWAYS 4 bytes (32 bits). The 32 bits are divided as:
            //     - 1 bit  : sign (positive or negative)
            //     - 8 bits : exponent (determines the magnitude/scale)
            //     - 23 bits: mantissa/fraction (determines the precision)
            
            //   For example, the float 0.35 is stored in memory as:
            //     Binary: 0 01111101 01100110011001100110011
            //     Hex:    3E B3 33 33
            //     That's exactly 4 bytes.
            
            //   WHAT WOULD CAUSE THE FLOAT TO CHANGE (CORRUPTION) IN TRANSIT?
            //     1) Sending wrong number of bytes (e.g., only 2 of the 4 bytes)
            //        → the receiver reconstructs a different bit pattern = wrong value
            //     2) Applying byte-order conversion (htonl/ntohl) to the float bytes
            //        → this would rearrange the bytes and corrupt the IEEE 754 layout
            //        → floats are NOT integers; byte-swapping changes their value
            //     3) Using a raw struct with compiler padding between fields
            //        → extra padding bytes shift the float's position in the buffer
            //     4) Not using memcpy for extraction (e.g., casting char* to float*)
            //        → this violates C++ strict aliasing rules and can cause
            //          undefined behavior where the compiler misreads the memory
            
            //   HOW FRAMING GUARANTEES CORRECTNESS:
            //     The header tells us Message Length = 4. We know to read EXACTLY
            //     4 bytes for the payload — no more, no less. We then memcpy those
            //     4 bytes into a float variable. Because we read the exact number
            //     of bytes and copy them without modification, the IEEE 754 bit
            //     pattern is preserved perfectly, and the float arrives intact.
            if (recv_length != sizeof(float)) {
                std::cerr << "[Server] Type 2 (Float) expects 4-byte payload, got " 
                          << recv_length << " bytes." << std::endl;
                delete[] recv_payload;
                break;
            }

            // Use memcpy to safely interpret the 4 raw bytes as a float.
            // We do NOT cast the pointer (e.g., *(float*)recv_payload) because
            // that violates strict aliasing rules and is undefined behavior.
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
        // SPRINT 2 EXPLANATION — Why we cannot send raw structs:
        
        //   If we defined a struct like:
        //     struct Header { uint32_t version; uint32_t type; uint32_t length; };
        //   and sent it with: send(fd, &header, sizeof(header), 0);
        
        //   This is UNSAFE because:
        //     1) PADDING: The compiler may insert invisible padding bytes between
        //        or after struct fields for alignment. sizeof(Header) might be
        //        16 bytes instead of 12. The receiver expects exactly 12.
        //     2) BYTE ORDER: The struct fields are in host byte order. If the
        //        sender is little-endian and receiver is big-endian, all values
        //        would be misinterpreted.
        //     3) NON-PORTABLE: Different compilers, platforms, and optimization
        //        levels may lay the struct out differently in memory.
        
        //   Instead, we manually build a byte buffer with memcpy(), placing each
        //   field at a known offset (0, 4, 8). This guarantees the wire format
        //   is exactly 12 bytes with fields in the correct positions.
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
    // SPRINT 1 EXPLANATION — socket():
    
    //   socket() creates a communication endpoint and returns a file descriptor.
    //   It does NOT connect to anything yet — it just allocates OS resources.
    
    //   Arguments:
    //     AF_INET     = use IPv4 addressing (as opposed to AF_INET6 for IPv6)
    //     SOCK_STREAM = use TCP (reliable, ordered, connection-oriented byte stream)
    //                   vs SOCK_DGRAM which would be UDP (unreliable, connectionless)
    //     0           = let the OS pick the default protocol for this socket type (TCP)
    
    //   Returns: a file descriptor (integer >= 0) on success, or -1 on failure.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Server] socket() creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure socket options:
    // SO_REUSEADDR lets us immediately rebind to the port after the process exits.
    // Without this, the OS keeps the port in TIME_WAIT state for ~60 seconds,
    // and we'd get "Address already in use" errors when restarting the server.
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[Server] setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // SPRINT 1 EXPLANATION — bind():
    
    //   bind() assigns a specific IP address and port number to the socket.
    //   Without bind(), the OS would assign a random port, and clients wouldn't
    //   know which port to connect to.
    
    //   INADDR_ANY means "listen on ALL available network interfaces" (not just
    //   localhost). This allows connections from other machines on the network.
    
    //   htons(PORT) converts the port number to network byte order (big-endian)
    //   because the sockaddr_in struct expects it in that format.
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

    // SPRINT 1 EXPLANATION — listen():
    
    //   listen() marks the socket as a PASSIVE socket — meaning it will be used
    //   to ACCEPT incoming connections, not to initiate outgoing ones.
    
    //   The BACKLOG parameter (5) sets the maximum number of pending connections
    //   that can wait in queue. If 5 clients try to connect before the server
    //   calls accept(), the 6th client's connection may be refused.
    
    //   After listen(), the socket is ready to receive connection requests,
    //   but no connection is established yet — that happens in accept().
    if (listen(server_fd, BACKLOG) < 0) {
        perror("[Server] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "[Server] Listening on port " << PORT << "...\n" << std::endl;

    // SPRINT 1 EXPLANATION — accept() in a loop:
    
    //   accept() BLOCKS (waits) until a client calls connect(). When a client
    //   connects, accept() creates a BRAND NEW socket (client_fd) specifically
    //   for communicating with that client. The original server_fd continues
    //   listening for more connections.
    
    //   This is crucial: server_fd is the "listening" socket that never sends
    //   or receives data. client_fd is the "connected" socket used for the
    //   actual data exchange with one specific client.
    
    //   The while(true) loop ensures the server does NOT exit after handling
    //   one client. After a client disconnects, we loop back to accept() and
    //   wait for the next client — making this a persistent server.
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
