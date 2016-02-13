CXX=g++
CC=gcc
CPPFLAGS=-std=c++11 -Wall -Wextra -pedantic -O3
CFLAGS=
LDFLAGS=-pthread \
	-lcppnetlib-uri \
    -lcppnetlib-server-parsers \
    -lcppnetlib-client-connections \
	-lboost_thread \
	-lboost_system \
    -lssl \
    -lcrypto \

SRC_DIR=src
DIST_DIR=dist

RM=rm -f

all: server

server: $(SRC_DIR)/server.o
	$(CXX) $(CPPFLAGS) $(SRC_DIR)/server.o -o $(DIST_DIR)/server $(LDFLAGS)
	@echo "\nserver built\n"

$(SRC_DIR)/server.o: $(SRC_DIR)/server.cpp $(SRC_DIR)/*.hpp
	$(CXX) $(CPPFLAGS) -c $(SRC_DIR)/server.cpp -o $(SRC_DIR)/server.o

clean:
	$(RM) $(SRC_DIR)/*.o $(DIST_DIR)/*
	@echo "\ncleaned"
