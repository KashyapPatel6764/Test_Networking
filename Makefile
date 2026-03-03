# Makefile – Sprint 3: LED Control Client-Server (C++ Version)
# CSC 4200, Program 1
#
# Targets:
#   all    – build both server and client (default)
#   server – build server binary
#   client – build client binary
#   clean  – remove compiled binaries

CXX      = g++
CXXFLAGS = -Wall -Wextra -g -std=c++11

.PHONY: all clean

all: server client

server: server.cpp protocol.h
	$(CXX) $(CXXFLAGS) -o server server.cpp

client: client.cpp protocol.h
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f server client

