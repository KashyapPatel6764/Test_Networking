// client.cpp - Sprint 3: Float Payload Transmission and Verification
// CSC 4200 - Program 1 (LED Control)

// Behaviour:
// - Connects to SERVER_IP:PORT
// - Constructs a 12-byte header (Version=17, Type=2, Length=4)
// - Uses htonl() to convert header fields to network byte order
// - Serializes a float value into a 4-byte payload using memcpy()
// - Sends the header, then sends the 4-byte float payload
// - Receives the echoed packet and verifies the returned float matches the original
// - Closes the socket cleanly

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

// Configuration
#define SERVER_IP "10.128.0.2"
#define PORT 9999

// SPRINT 1 EXPLANATION — Why send() may not send all bytes:

//   send() hands bytes to the OS kernel's TCP send buffer. If the buffer is
//   full or the network is slow, send() may accept fewer bytes than requested
//   and return a smaller count. We must loop and send again the remaining bytes.

//   The difference between send() and recv():
//     send() = PUSH bytes into the network (to the server)
//     recv() = PULL bytes from the network (from the server)
//   Neither of them guarantees transferring the exact number of bytes you request
//   in a single call. You must always check the return.
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

// SPRINT 1 EXPLANATION — Why recv() may return fewer bytes than requested:

//   TCP is a byte stream. It does NOT preserve message boundaries. If the
//   server sends 16 bytes (12-byte header + 4-byte payload), the client's
//   recv() might return 8 bytes on the first call and 8 bytes on the second.
//   Or it might return all 16 at once. There is no guarantee.

//   WHAT DOES recv() RETURNING 0 MEAN?
//     It means the server has closed the connection (called close() on its
//     socket). This is NOT an error it is TCP's way of saying end of data
//     We must detect this and stop reading data.

//   That is why we loop in recv_all() until we have received exactly the number
//   of bytes we need or detect a disconnect (return 0) or error (return -1).
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
            // recv() returned 0 → server closed the connection gracefully.
            return 0;
        }
        total_recv += static_cast<size_t>(n);
    }
    // Success, returned total bytes read
    return total_recv; 
}

int main()
{
    
    // SPRINT 1 EXPLANATION — socket():

    //   socket() creates a communication endpoint. It does NOT connect anywhere.
    //   It just reserves OS resources and returns a file descriptor.
    
    //   AF_INET      = IPv4 Internet protocol
    //   SOCK_STREAM  = TCP (reliable, ordered, connection-oriented)
    //   0            = default protocol for the given type (TCP for SOCK_STREAM)
    int local_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (local_sock < 0) {
        perror("[Client] socket creation failed");
        exit(EXIT_FAILURE);
    }

    // SPRINT 1 EXPLANATION — Setting up the server address:
    
    //   We fill in a sockaddr_in struct with the server's IP and port.
    //   htons(PORT) converts the port number to network byte order.
    //   inet_pton() converts the IP string "10.128.0.2" to its binary form.
    //   This struct tells connect() WHERE to establish the TCP connection.
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

    // SPRINT 1 EXPLANATION — connect():
    
    //   connect() initiates a TCP three-way handshake with the server:
    //     1) Client sends SYN to server
    //     2) Server responds with SYN-ACK
    //     3) Client sends ACK
    
    //   After this completes, a reliable, bidirectional byte stream is
    //   established between client and server. Both sides can now send()
    //   and recv() data.
    
    //   connect() blocks until the handshake completes or fails (e.g., if
    //   the server is not running or the port is wrong).
    if (connect(local_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Client] connection to server failed");
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    std::cout << "[Client] Successfully connected to " << SERVER_IP << ":" << PORT << std::endl;

    // SPRINT 3 EXPLANATION — Why the float payload is exactly 4 bytes:
    
    //   A C/C++ float follows IEEE 754 single-precision format = 32 bits = 4 bytes.
    //   The 32 bits encode: 1 sign bit + 8 exponent bits + 23 mantissa bits.
    
    //   For example, 0.35 is stored as hex: 3E B3 33 33 (4 bytes).
    //   That is why Message Length = 4 for a float payload.
    
    //   We do NOT apply htonl() to the float bytes. htonl() is only for
    //   unsigned integers in the header. Byte-swapping a float's raw bytes
    //   would rearrange its IEEE 754 bit pattern and CORRUPT the value.
    //   The float is copied as-is using memcpy().
    float send_float = 0.35f;
    uint32_t payload_len = sizeof(float); // 4 bytes

    std::cout << "[Client] Preparing to send float value: " << send_float << std::endl;
    // SPRINT 2 EXPLANATION — Why we use htonl() before sending header fields:
    
    //   Multi-byte integers (like uint32_t) are stored differently on different
    //   CPUs. x86/x64 uses little-endian; some ARM and network protocols use
    //   big-endian. The TCP/IP standard mandates that all protocol fields use
    //   big-endian ("network byte order").
    
    //   htonl() = "Host TO Network Long" — converts a 32-bit integer from
    //   whatever byte order the host CPU uses to big-endian. This guarantees
    //   that the server reads the correct values regardless of architecture.
    
    //   Without htonl(), if we send Version=17 from a little-endian machine,
    //   the server on a big-endian machine would read it as 285,212,672.
    uint32_t net_version = htonl(VERSION);
    uint32_t net_type    = htonl(TYPE_FLOAT);  // Type 2 for float
    uint32_t net_length  = htonl(payload_len); // Length = 4

    // SPRINT 2 EXPLANATION — Why we use memcpy() instead of sending a raw struct:
    
    //   If we defined: struct Header { uint32_t v, t, l; };
    //   and did: send(fd, &header, sizeof(header), 0);
    
    //   This is DANGEROUS because:
    //     1) COMPILER PADDING: The compiler may insert invisible padding bytes
    //        between struct fields for memory alignment. sizeof(Header) could
    //        be 16 instead of 12, and the extra bytes would corrupt the protocol.
    //     2) NON-PORTABLE: Different compilers, OSes, and optimization flags
    //        may arrange struct memory differently.
    //     3) NO BYTE ORDER CONTROL: Raw struct bytes are in host byte order.
    
    //   Instead, we manually pack each 4-byte field into a buffer at exact
    //   offsets (0, 4, 8) using memcpy(). This guarantees the wire format is
    //   exactly 12 bytes with no padding and fields in the correct positions.
    char header_buffer[HEADER_SIZE];
    
    // Copy each field into the header buffer at exact byte offsets
    memcpy(header_buffer, &net_version, 4);       // Bytes 0-3:  Version
    memcpy(header_buffer + 4, &net_type, 4);      // Bytes 4-7:  Message Type
    memcpy(header_buffer + 8, &net_length, 4);    // Bytes 8-11: Message Length

    // SPRINT 3 EXPLANATION — Float serialization with memcpy():
    
    //   We copy the float's raw 4 bytes into a char buffer using memcpy().
    //   This preserves the exact IEEE 754 bit pattern without modification.
    
    //   WHAT WOULD CAUSE THE FLOAT VALUE TO CHANGE (CORRUPTION)?
    //     1) Applying htonl() to the float bytes — this byte-swaps the IEEE 754
    //        bit pattern, producing a completely different (wrong) float value.
    //     2) Sending fewer than 4 bytes — the receiver would reconstruct the
    //        float from incomplete bytes, getting a garbage value.
    //     3) Sending a raw struct — compiler padding could shift the float's
    //        position in the buffer, causing the receiver to read wrong bytes.
    //     4) Pointer casting instead of memcpy — aliasing violations can cause
    //        undefined behavior where the compiler misinterprets the memory.
    
    //   HOW FRAMING GUARANTEES CORRECTNESS:
    //     The header says Message Length = 4. The receiver reads EXACTLY 4 bytes
    //     for the payload, then memcpy's them into a float. Because we send and
    //     receive the exact byte count and copy without modification, the
    //     IEEE 754 bit pattern is preserved end-to-end.
    char payload_buffer[sizeof(float)];
    memcpy(payload_buffer, &send_float, sizeof(float));

    // Send the 12-byte header
    if (send_all(local_sock, header_buffer, HEADER_SIZE) < 0) {
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    
    // Send the 4-byte float payload
    if (send_all(local_sock, payload_buffer, payload_len) < 0) {
        close(local_sock);
        exit(EXIT_FAILURE);
    }
    std::cout << "[Client] Sent header (Type=2, Length=4) and float payload." << std::endl;

    // Receiving the server's echoed response:
    
    // SPRINT 2 — recv() must be looped (handled by recv_all):
    //   The server sends back 12 header bytes + 4 payload bytes = 16 bytes.
    //   TCP might deliver these in any number of chunks. recv_all() loops
    //   until we have EXACTLY 12 bytes for the header, then EXACTLY 4 bytes
    //   for the payload as determined by the Message Length field.
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

    // Extract header fields using memcpy (safe, no pointer aliasing issues)
    uint32_t recv_net_version, recv_net_type, recv_net_length;
    memcpy(&recv_net_version, recv_header, 4);
    memcpy(&recv_net_type, recv_header + 4, 4);
    memcpy(&recv_net_length, recv_header + 8, 4);

    // Convert from network byte order (big-endian) to host byte order using ntohl()
    uint32_t recv_version = ntohl(recv_net_version);
    uint32_t recv_type    = ntohl(recv_net_type);
    uint32_t recv_length  = ntohl(recv_net_length);

    std::cout << "[Client] Received Header - Version: " << recv_version 
              << ", Type: " << recv_type 
              << ", Length: " << recv_length << std::endl;

    // SPRINT 2 — Using Message Length for framing:
    //   recv_length tells us EXACTLY how many payload bytes to expect.
    //   Without this field, we would have no way to know where the payload
    //   ends in the TCP byte stream. This is protocol-level framing.
    char recv_payload[sizeof(float)];
    memset(recv_payload, 0, sizeof(float));

    int payload_bytes = recv_all(local_sock, recv_payload, recv_length);
    if (payload_bytes < 0) {
        close(local_sock);
        exit(EXIT_FAILURE);
    }

    // Extract the float from the received payload using memcpy (safe deserialization)
    float recv_float;
    memcpy(&recv_float, recv_payload, sizeof(float));

    std::cout << "[Client] Received float value: " << recv_float << std::endl;

    // SPRINT 3 — Float verification:
    //   If the sent float (0.35) matches the received float, it proves that:
    //     - The header was constructed and parsed correctly
    //     - The 4-byte IEEE 754 bit pattern was preserved through transmission
    //     - No byte-order corruption occurred (we didn't htonl() the float)
    //     - Framing worked: we read exactly 4 bytes, no more, no less
    //   If they DON'T match, something in the byte handling is broken.
    if (recv_float == send_float) {
        std::cout << "[Client] Verification SUCCESS: Sent float (" << send_float 
                  << ") matches received float (" << recv_float << ")." << std::endl;
    } else {
        std::cout << "[Client] Verification FAILED: Sent float (" << send_float 
                  << ") does NOT match received float (" << recv_float << ")." << std::endl;
    }

    // 6. Close socket cleanly
    close(local_sock);
    std::cout << "[Client] Socket closed. Client exiting." << std::endl;

    return 0; // Success
}
