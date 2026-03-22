# MiniCoin — Decentralizovaná mena (demo)
#
# Závislosti: OpenSSL (libssl-dev / openssl-devel), pthreads
#
# Použitie:
#   make          — skompilovať
#   make clean    — vyčistiť
#   make run      — spustiť na predvolenom porte (9333)
#   make run2     — spustiť druhý node na porte 9334

CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -O2 -g -I include
LDFLAGS  = -lssl -lcrypto -lpthread

SRC_DIR  = src
OBJ_DIR  = build/obj
BIN_DIR  = build/bin
BIN      = $(BIN_DIR)/minicoin

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean run run2

all: $(BIN)

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf build wallet.pem

run: $(BIN)
	./$(BIN) 9333

run2: $(BIN)
	./$(BIN) 9334
