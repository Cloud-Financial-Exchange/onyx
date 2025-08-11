SRCS-proxy := ./multicast_proxy/multicast_proxy.cpp ./multicast_proxy/dpdk_proxy.hpp ./multicast_proxy/iouring_proxy.hpp ./multicast_proxy/socket_proxy.hpp
SRCS-receivers := ./multicast_receiver/multicast_receivers.cpp ./multicast_receiver/dpdk_receiver.hpp ./multicast_receiver/socket_receiver.hpp
SRCS-client := ./multicast_client/multicast_client.cpp
SRCS-common := utils/common.hpp utils/tree.hpp ./build/message.pb.cc ./build/message.pb.h ./message.h utils/ZMQSocket.hpp utils/ZMQSocket.cpp
SRCS-proto := message.proto

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -Wno-missing-field-initializers -DZMQ_BUILD_DRAFT_API=1
LDFLAGS = -luring -lpthread -L/usr/lib -lbpf
PC_FILE := $(shell pkg-config --path libdpdk)
ZMQ_FLAGS := $(shell pkg-config --libs --cflags cppzmq) 

PROTOC = protoc
PROTOC_FLAGS = --cpp_out=.

.PHONY: all multicast_client multicast_proxy multicast_receivers multicast_protobufs clean

all: multicast_protobufs multicast_client multicast_proxy multicast_receivers

multicast_proxy: $(SRCS-proxy) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-proxy) $(SRCS-common) -o ./build/$@ $(LDFLAGS) `pkg-config --cflags --libs protobuf` `pkg-config --cflags --libs libdpdk` $(ZMQ_FLAGS)

multicast_receivers: $(SRCS-receivers) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-receivers) $(SRCS-common) -o ./build/$@ $(LDFLAGS) `pkg-config --cflags --libs protobuf` `pkg-config --cflags --libs libdpdk` $(ZMQ_FLAGS)

multicast_client: $(SRCS-client)
	$(CC) $(CFLAGS) $(SRCS-client) $(SRCS-common) -o ./build/$@ $(LDFLAGS) `pkg-config --cflags --libs protobuf` $(ZMQ_FLAGS)

multicast_protobufs: $(SRCS-proto)
	protoc $(SRCS-proto) --cpp_out=./build/

clean:
	rm -f ./build/*
