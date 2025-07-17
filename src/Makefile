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

COLOR_R 	:= "\e[31m"
COLOR_G 	:= "\e[32m"
COLOR_B		:= "\e[33m"
COLOR_END 	:= "\e[0m"

# +----------------------+
# | Compilation of files |
# +----------------------+

# Compile C files to object files.
$(OBJS_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo $(COLOR_B)"Compiling $< -> $@"$(COLOR_END)
	$(CC) $(FLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Linking into final executable.
$(TARGET): $(OBJS)
	@echo $(COLOR_B)"Linking $(TARGET)."$(COLOR_END)
	$(CC) $(OBJS) -o $@

# +--------------+
# | Main targets |
# +--------------+

.PHONY: all debug clean

all: $(TARGET)

# Will recreate directories for object files and print informations.
debug:
	@mkdir -p obj/vdisk
	@echo $(COLOR_G)"SRC_DIR : "	$(COLOR_END)	$(SRCS_DIR)
	@echo $(COLOR_G)"SRCS : "		$(COLOR_END)	$(SRCS)
	@echo $(COLOR_G)"OBJS_DIR : "	$(COLOR_END)	$(OBJS_DIR)
	@echo $(COLOR_G)"OBJS : "		$(COLOR_END)	$(OBJS)

clean:
	@rm -f $(TARGET)
	@rm -rf $(OBJS_DIR)
	@echo $(COLOR_R)"Cleaned obj/, target."$(COLOR_END)
