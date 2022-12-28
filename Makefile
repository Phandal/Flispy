CC = cc
CFLAGS = -Wall -Wextra
LDFLAGS = -ledit -lm
BIN = flispy

SRCDIR = src
LIBDIR = lib
OBJS = $(SRCDIR)/main.o $(LIBDIR)/mpc.o

.PHONY: all
all: clean build

.PHONY: build
build: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $^

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean
clean:
	-rm -rf $(OBJS)
