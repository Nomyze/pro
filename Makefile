# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Target program
TARGET = memthing

# Source and object files
SRCS = src/main.c src/mem/proc.c
OBJS = $(SRCS:.c=.o)

# Default rule
all: $(TARGET)

# Link step: build the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile step: .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

