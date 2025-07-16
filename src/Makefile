# C Compiler.
CC := gcc

# Install libbsd-dev prior to compiling
FLAGS := -Wall -pedantic -std=c99 -Wextra -D_POSIX_SOURCE -lbsd -g

# Final executable name.
TARGET := fs_test

# Directory containing all C header files.
INCLUDE_DIR := include

# Finding every C source file in the current and subsequent directories.
SRCS_DIR := .
SRCS := $(shell find $(SRCS_DIR) -name "*.c")

# All objects files will be dumped into a dedicated directory.
OBJS_DIR := obj
OBJS := $(patsubst $(SRCS_DIR)/%.c,$(OBJS_DIR)/%.o,$(SRCS))  # .file.c => ./obj/file.o

# Create the object directory if it doesn't exist
$(OBJS_DIR):
	@mkdir -p $(OBJS_DIR)
	@mkdir -p $(OBJS_DIR)/vdisk
	@echo "Created obj/ directory for dumping .o files."

# Compile C files to object files.
$(OBJS_DIR)/%.o: %.c | $(OBJS_DIR)
	@echo "Compiling $< -> $@"
	$(CC) $(FLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)."
	$(CC) $(OBJS) -o $@

.PHONY: all clean test

all: $(TARGET)

test:
	@echo "SRC_DIR : " 	$(SRCS_DIR)
	@echo "SRCS : " 	$(SRCS)
	@echo "OBJS_DIR : " 	$(OBJS_DIR)
	@echo "OBJS :" 		$(OBJS)


clean:
	@rm -f $(TARGET)
	@rm -rf $(OBJS_DIR)
	@echo "Cleaned obj/, target"
