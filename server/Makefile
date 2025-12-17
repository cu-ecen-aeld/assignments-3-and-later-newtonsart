# Makefile for aesdsocket server
# Supports cross-compilation via CROSS_COMPILE variable

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?=

TARGET = aesdsocket
SRC = aesdsocket.c
OBJS = $(SRC:.c=.o)

.PHONY: all default clean

all: $(TARGET)

default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f $(OBJS) $(TARGET)
