# compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Werror

# source files
SRCS = bus.c \
       cartridge.c \
       cpu.c \
       cpu-fetch.c \
       cpu-process.c \
       cpu-utilities.c \
       debug.c \
       dma.c \
       emulator.c \
       gamepad.c \
       instructions.c \
       interrupts.c \
       io.c \
       lcd.c \
       main.c \
       ppu.c \
       ppu-pipeline.c \
       ppu-sm.c \
       ram.c \
       stack.c \
       timer.c \
       ui.c \


OBJS = $(SRCS:.c=.o)

# executable file
TARGET = gbemu

# default rule
all: $(TARGET)

# link the object files to create binary
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) -lSDL2 -lSDL2_ttf -pthread

# compile .cpp files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# clean up object files and binary
clean:
	rm -rf $(OBJS) $(TARGET)

.PHONY: all clean
