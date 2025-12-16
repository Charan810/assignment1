# Makefile
# Builds:
#   anim       from tcpclient.cpp
#   udp_client from udpclient.cpp

CXX     = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

all: build

build: anim udp_client

anim: tcpclient.cpp
	$(CXX) $(CXXFLAGS) tcpclient.cpp -o anim

udp_client: udpclient.cpp
	$(CXX) $(CXXFLAGS) udpclient.cpp -o udp_client -lcrypto

clean:
	rm -f anim udp_client

.PHONY: all build clean
