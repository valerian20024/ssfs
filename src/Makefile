# +--------------------+
# | Makefile Variables |
# +--------------------+

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

# +----------------------+
# | Compilation of files |
# +----------------------+

# Compile C files to object files.
$(OBJS_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "Compiling $< -> $@"
	$(CC) $(FLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Linking into final executable.
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)."
	$(CC) $(OBJS) -o $@

# +--------------+
# | Main targets |
# +--------------+

.PHONY: all debug clean

all: $(TARGET)

# Will recreate directories for object files and print informations.
debug:
	@mkdir -p obj/vdisk
	@echo "SRC_DIR : "	$(SRCS_DIR)
	@echo "SRCS : "		$(SRCS)
	@echo "OBJS_DIR : "	$(OBJS_DIR)
	@echo "OBJS : "		$(OBJS)

clean:
	@rm -f $(TARGET)
	@rm -rf $(OBJS_DIR)
	@echo "Cleaned obj/, target."
