#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstddef>

// This file defines the constants and structure logic for the binary protocol header.
// It ensures that both the client and server agree on the version, types, and header size.

// The protocol version required by the assignment constraints.
const uint32_t VERSION = 17;

// Message types
const uint32_t TYPE_ECHO  = 1;   
const uint32_t TYPE_FLOAT = 2;   

// The fixed size of the header in bytes (Version: 4 Bytes, Type: 4 Bytes, Length: 4 Bytes).
const size_t HEADER_SIZE = 12;

#endif // PROTOCOL_H
