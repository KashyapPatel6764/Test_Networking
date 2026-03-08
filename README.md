# Program 1 – LED Control (Client-Server Communication)

**CSC 4200 – Computer Networks**
**Author:** Kashyap Patel

---

## Overview

This project implements a remote-controlled LED communication system using TCP sockets in C++. A client sends structured binary packets to a server following a custom-defined protocol. The server parses, validates, and echoes back the received data. The system simulates the efficient, compact binary communication used in real-world embedded and space systems.

---

## Project Structure

```
assignment-1-simple-client-server/
├── server.cpp      # Server implementation (binds, listens, parses, echoes)
├── client.cpp      # Client implementation (constructs packets, sends, verifies)
├── protocol.h      # Shared protocol constants (VERSION, message types, header size)
├── Makefile        # Build configuration for compiling server and client
├── assignment-1.md # Full assignment specification
└── README.md       # This file
```

---

## Protocol Specification

All communication follows a fixed binary protocol. Each packet consists of a **12-byte header** followed by a variable-length **payload**.

### Header Format (12 bytes)

| Field           | Size    | Description                              |
|-----------------|---------|------------------------------------------|
| Version         | 4 bytes | Protocol version (must be `17`)          |
| Message Type    | 4 bytes | `1` = String Echo, `2` = Float Echo      |
| Message Length  | 4 bytes | Length of the payload in bytes            |

All header fields are transmitted in **network byte order** (big-endian) using `htonl()` / `ntohl()`.

### Packet Diagram

```
+---------------------+----------------------+----------------------+
| Version (4 bytes)   | Message Type (4B)    | Message Length (4B)  |
+---------------------+----------------------+----------------------+
|                      Payload (N bytes)                           |
+------------------------------------------------------------------+
```

---

## Sprint Breakdown

### Sprint 1 – Basic TCP Communication
- Established a reliable TCP connection between client and server.
- Server creates a socket, binds to port `9999`, listens, and accepts connections.
- Client connects, sends a test message, receives a response.
- Server continues running after each client disconnects.
- All return values from `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, and `send()` are checked.

### Sprint 2 – Header Generation & Parsing
- Implemented the 12-byte binary header with `Version`, `Message Type`, and `Message Length`.
- Client constructs the header manually using `memcpy()` (no raw struct transmission).
- Server receives exactly 12 bytes, decodes fields with `ntohl()`, and validates `Version == 17`.
- Payload is read based on the `Message Length` field.
- Server echoes the full packet (header + payload) back to the client.

### Sprint 3 – Floating-Point Payload
- Client sends a `float` value (e.g., `0.35`) as a 4-byte payload with `Message Type = 2`.
- Server receives, interprets, prints, and echoes the float back.
- Client verifies the returned float matches the original sent value.
- Safe serialization/deserialization using `memcpy()` (no type-punning or aliasing issues).

---

## Building & Running

### Prerequisites
- **Linux environment** (tested on GCP Compute Engine)
- `g++` compiler with C++11 support
- `make`

### Compile

```bash
make          # Builds both server and client
make server   # Builds only the server
make client   # Builds only the client
make clean    # Removes compiled binaries
```

### Run

**Terminal 1 – Start the Server:**
```bash
./server
```
Expected output:
```
[Server] Listening on port 9999...
```

**Terminal 2 – Run the Client:**
```bash
./client
```
Expected output:
```
[Client] Successfully connected to 10.128.0.2:9999
[Client] Preparing to send float value: 0.35
[Client] Sent header (Type=2, Length=4) and float payload.
[Client] Received Header - Version: 17, Type: 2, Length: 4
[Client] Received float value: 0.35
[Client] Verification SUCCESS: Sent float (0.35) matches received float (0.35).
[Client] Socket closed. Client exiting.
```

> **Note:** The `SERVER_IP` in `client.cpp` is set to `10.128.0.2` (GCP internal IP). Update this to `127.0.0.1` for local testing or to your server's IP for remote testing.

---

## Key Implementation Details

- **`send_all()` / `recv_all()`** – Helper functions that loop on `send()` and `recv()` to handle TCP's stream-based nature, ensuring all bytes are transmitted/received.
- **`memcpy()` for serialization** – All header fields and payloads are manually packed into byte buffers. Raw structs are never sent over the wire to avoid compiler padding and alignment issues.
- **Network byte order** – `htonl()` converts host integers to big-endian before sending; `ntohl()` converts back after receiving. This ensures cross-platform compatibility.
- **Graceful disconnection** – The server detects `recv()` returning `0` (client disconnect) and continues accepting new connections.

---

## Configuration

| Constant     | Value          | Location      | Description                        |
|-------------|----------------|---------------|------------------------------------|
| `VERSION`   | `17`           | `protocol.h`  | Required protocol version          |
| `TYPE_ECHO` | `1`            | `protocol.h`  | Message type for string echo       |
| `TYPE_FLOAT`| `2`            | `protocol.h`  | Message type for float echo        |
| `HEADER_SIZE`| `12`          | `protocol.h`  | Fixed header size in bytes         |
| `PORT`      | `9999`         | `server.cpp` / `client.cpp` | TCP port       |
| `SERVER_IP` | `10.128.0.2`   | `client.cpp`  | Server address (update as needed)  |

---

## Verification with Wireshark

Wireshark can be used to inspect packets and confirm:
- The 12-byte header is transmitted correctly.
- Fields are in **big-endian** (network) byte order.
- The payload length matches the `Message Length` field.
- Float payloads are exactly 4 bytes.

---

## References

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/split-wide/)
- CSC 4200 – Assignment 1 Specification (`assignment-1.md`)
