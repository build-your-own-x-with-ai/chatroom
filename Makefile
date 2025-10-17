CC = gcc
CFLAGS = -Wall -Wextra -I./include -pthread
SDL_FLAGS = `sdl2-config --cflags --libs` -lSDL2_ttf

SRC_DIR = src
BIN_DIR = bin
INCLUDE_DIR = include

SERVER_SRC = $(SRC_DIR)/server.c
CLIENT_SRC = $(SRC_DIR)/client.c

SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/client

.PHONY: all clean server client

all: $(BIN_DIR) server client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(INCLUDE_DIR)/common.h
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)

client: $(CLIENT_BIN)

$(CLIENT_BIN): $(CLIENT_SRC) $(INCLUDE_DIR)/common.h
	$(CC) $(CFLAGS) $(CLIENT_SRC) $(SDL_FLAGS) -o $(CLIENT_BIN)

clean:
	rm -rf $(BIN_DIR)

run-server: server
	./$(SERVER_BIN)

run-client: client
	@echo "Usage: ./$(CLIENT_BIN) <username> <server_ip>"
	@echo "Example: ./$(CLIENT_BIN) Alice 127.0.0.1"
