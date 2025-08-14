# +--------------------+
# | Makefile Variables |
# +--------------------+

# C Compiler.
CC := gcc

# Install libbsd-dev prior to compiling
FLAGS := -Wall -pedantic -std=c99 -Wextra -D_POSIX_SOURCE -lbsd -g -gdwarf-4

# Final executable name.
TARGET := fs_test

# Submission archive name
ARCHIVE := src.tar.gz

# Directory containing all C header files.
INCLUDE_DIR := include

# Finding every C source file in the current and subsequent directories.
SRCS_DIR := .
SRCS := $(shell find $(SRCS_DIR) -name "*.c")

# All objects files will be dumped into a dedicated directory.
OBJS_DIR := obj
OBJS := $(patsubst $(SRCS_DIR)/%.c,$(OBJS_DIR)/%.o,$(SRCS))  # .file.c => ./obj/file.o

# Temporary directory where source code is dumped to create the submission archive. 
BUILD_DIR := build

# Directory used to put the output of the program execution
OUTPUT_DIR := output

# Defining ANSI colors for clearer output
COLOR_R 	:= "\e[31m"
COLOR_G 	:= "\e[32m"
COLOR_Y		:= "\e[33m"
COLOR_END 	:= "\e[0m"

# +--------------+
# | Main targets |
# +--------------+

all: $(TARGET)
	@mkdir -p $(OUTPUT_DIR)

# +----------------------+
# | Compilation of files |
# +----------------------+

# Check libs dependencies
check-libs:
	@echo -e $(COLOR_Y)"Checking for libbsd..."$(COLOR_END);
	@if command -v apt-get >/dev/null 2>&1; then \
		if dpkg -l | grep -q libbsd-dev; then \
			echo -e $(COLOR_G)"libbsd-dev is installed (Debian/Ubuntu)"$(COLOR_END); \
		else \
			echo -e "Installing libbsd-dev..."; \
			sudo apt-get update && sudo apt-get install -y libbsd-dev || { \
				echo -e $(COLOR_R)"Failed to install libbsd-dev. Please install it manually."$(COLOR_END); \
				exit 1; \
			}; \
			echo -e $(COLOR_G)"libbsd-dev installed successfully"$(COLOR_END); \
		fi; \
	elif command -v pacman >/dev/null 2>&1; then \
		if pacman -Q libbsd >/dev/null 2>&1; then \
			echo -e $(COLOR_G)"libbsd is installed (Arch/Manjaro)"$(COLOR_END); \
		else \
			echo -e "Installing libbsd..."; \
			sudo pacman -S --noconfirm libbsd || { \
				echo -e $(COLOR_R)"Failed to install libbsd. Please install it manually."$(COLOR_END); \
				exit 1; \
			}; \
			echo -e $(COLOR_G)"libbsd installed successfully"$(COLOR_END); \
		fi; \
	else \
		echo -e $(COLOR_R)"No supported package manager (apt-get or pacman) found."$(COLOR_END); \
		echo -e "Please ensure libbsd is installed manually."; \
		exit 1; \
	fi

# Compile C files to object files.
$(OBJS_DIR)/%.o: %.c | check-libs
	@mkdir -p $(@D)
	@echo -e $(COLOR_Y)"Compiling $< -> $@"$(COLOR_END)
	@$(CC) $(FLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Linking into final executable.
$(TARGET): $(OBJS) | check-libs
	@echo -e $(COLOR_Y)"Linking $(TARGET)."$(COLOR_END)
	@$(CC) $(OBJS) -o $@ -lbsd

# +---------------+
# | Other targets |
# +---------------+

.PHONY: all debug clean build check-libs show

# Will recreate directories for object files and print informations.
debug:
	@mkdir -p obj/vdisk
	@echo -e $(COLOR_G)"SRC_DIR : "	$(COLOR_END)	$(SRCS_DIR)
	@echo -e $(COLOR_G)"SRCS : "		$(COLOR_END)	$(SRCS)
	@echo -e $(COLOR_G)"OBJS_DIR : "	$(COLOR_END)	$(OBJS_DIR)
	@echo -e $(COLOR_G)"OBJS : "		$(COLOR_END)	$(OBJS)
	@echo -e $(COLOR_Y)"Compiled using :" $(COLOR_END)
	@echo -e $(CC) $(FLAGS) -I$(INCLUDE_DIR) -c -o
	@echo -e $(COLOR_Y)"Linked using :" $(COLOR_END)
	@echo -e $(CC) $(OBJS) -o

clean:
	@rm -f $(TARGET)
	@rm -rf $(ARCHIVE)
	@rm -rf $(OBJS_DIR)
	@rm -rf $(BUILD_DIR)
	@rm -rf $(TEST_DIR)
	@echo -e $(COLOR_R)"Cleaned:" $(COLOR_END)
	@echo -e "  $(TARGET)"
	@echo -e "  $(ARCHIVE)"
	@echo -e "  $(OBJS_DIR)/"
	@echo -e "  $(BUILD_DIR)/"
	@echo -e "  $(TEST_DIR)/"

# Create a submission archive with source files, Makefile, include, and vdisk directories inside a src/ folder
build:
	@echo -e $(COLOR_Y)"Creating submission archive src.tar.gz with src/ folder"$(COLOR_END)
	@rm -rf build
	@mkdir -p build/src
	@cp -r *.c build/src/
	@cp Makefile build/src/
	@cp -r include build/src/
	@cp -r vdisk build/src/
	@tar -czf $(ARCHIVE) -C build src
	@rm -rf build
	@echo -e $(COLOR_G)"Submission archive src.tar.gz created successfully"$(COLOR_END)

show:
	@echo -e $(COLOR_Y)"Restoring raw files from hex dumps in $(OUTPUT_DIR)"$(COLOR_END)
	for f in $(OUTPUT_DIR)/*.hex; do \
		[ -e "$$f" ] || continue; \
		outfile=$${f%.hex}.raw; \
		xxd -r -p "$$f" "$$outfile"; \
		echo -e "Restored $$outfile from $$f"; \
	done
