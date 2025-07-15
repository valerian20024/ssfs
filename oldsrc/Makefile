# Compiler and flags
CC = gcc
CFLAGS = -I./include -Wall -pedantic -std=c99 -Wextra -D_POSIX_SOURCE -g
LDFLAGS = -lbsd

# Directories
OBJ_DIR = obj
INCLUDE_DIR = include
VDISK_DIR = vdisk

# Source files
SRCS = ./$(VDISK_DIR)/vdisk.c ./error.c ./fs.c ./main.c

# Building the object files locations. All object files will be placed in the obj directory. 
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))

# Target executable
TARGET = fs_test

# Default make target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
# vdisk is traeted differently because it is in a subdirectory
$(OBJ_DIR)/%.o: ./vdisk/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: ./%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create the object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# Phony targets
.PHONY: all clean