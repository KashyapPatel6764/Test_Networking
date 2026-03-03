#ifndef PROTOCOL_H
#define PROTOCOL_H

// This file defines the constants and structure logic for the Sprint 2 binary protocol header.
// It ensures that both the client and server agree on the version, type, and header size.

// The protocol version required by the assignment constraints.
const uint32_t VERSION = 17;

// Identifying the type of message being sent. We will use type 1 for this sprint.
const uint32_t TYPE_ECHO = 1;

// The fixed size of the header in bytes (Version: 4 Bytes, Type: 4 Bytes, Length: 4 Bytes).
const size_t HEADER_SIZE = 12;

#endif // PROTOCOL_H
