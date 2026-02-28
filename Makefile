# Makefile – Sprint 1: Basic TCP Client-Server (C++ Version)
# CSC 4200, Program 1 (LED Control)
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

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f server client
