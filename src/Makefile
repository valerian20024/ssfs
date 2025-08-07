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

# Defining ANSI colors for clearer output
COLOR_R 	:= "\e[31m"
COLOR_G 	:= "\e[32m"
COLOR_Y		:= "\e[33m"
COLOR_END 	:= "\e[0m"

# +--------------+
# | Main targets |
# +--------------+

all: $(TARGET)

# +----------------------+
# | Compilation of files |
# +----------------------+

# Check libs dependencies 
check-libs:
	@echo $(COLOR_Y)"Checking for libbsd-dev..."$(COLOR_END);
	@dpkg -l | grep -q libbsd-dev || { \
		echo "Installing libbsd-dev..."; \
		sudo apt-get update && sudo apt-get install -y libbsd-dev; \
	}
	@echo $(COLOR_G)"libbsd-dev is installed"$(COLOR_END);

# Compile C files to object files.
$(OBJS_DIR)/%.o: %.c | check-libs
	@mkdir -p $(@D)
	@echo $(COLOR_Y)"Compiling $< -> $@"$(COLOR_END)
	@$(CC) $(FLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Linking into final executable.
$(TARGET): $(OBJS) | check-libs
	@echo $(COLOR_Y)"Linking $(TARGET)."$(COLOR_END)
	@$(CC) $(OBJS) -o $@ -lbsd

# +---------------+
# | Other targets |
# +---------------+

.PHONY: all debug clean build check-libs show

# Will recreate directories for object files and print informations.
debug:
	@mkdir -p obj/vdisk
	@echo $(COLOR_G)"SRC_DIR : "	$(COLOR_END)	$(SRCS_DIR)
	@echo $(COLOR_G)"SRCS : "		$(COLOR_END)	$(SRCS)
	@echo $(COLOR_G)"OBJS_DIR : "	$(COLOR_END)	$(OBJS_DIR)
	@echo $(COLOR_G)"OBJS : "		$(COLOR_END)	$(OBJS)
	@echo $(COLOR_Y)"Compiled using :" $(COLOR_END)
	@echo $(CC) $(FLAGS) -I$(INCLUDE_DIR) -c -o
	@echo $(COLOR_Y)"Linked using :" $(COLOR_END)
	@echo $(CC) $(OBJS) -o

clean:
	@rm -f $(TARGET)
	@rm -rf $(ARCHIVE)
	@rm -rf $(OBJS_DIR)
	@rm -rf $(BUILD_DIR)
	@echo $(COLOR_R)"Cleaned:" $(TARGET) $(ARCHIVE) $(OBJS_DIR)/ $(BUILD_DIR)/$(COLOR_END)

# Create a submission archive with source files, Makefile, include, and vdisk directories inside a src/ folder
build:
	@echo $(COLOR_Y)"Creating submission archive src.tar.gz with src/ folder"$(COLOR_END)
	@rm -rf build
	@mkdir -p build/src
	@cp -r *.c build/src/
	@cp Makefile build/src/
	@cp -r include build/src/
	@cp -r vdisk build/src/
	@tar -czf $(ARCHIVE) -C build src
	@rm -rf build
	@echo $(COLOR_G)"Submission archive src.tar.gz created successfully"$(COLOR_END)

show:
	@mkdir -p test
	@TIME=$$(date +"%Y%m%d_%H%M%S"); \
	echo $(COLOR_Y)"Running $(TARGET), output to ./test/output_$${TIME}.txt"$(COLOR_END); \
	./$(TARGET) > ./test/output_$${TIME}.txt 2>&1; \
	echo $(COLOR_G)"Output saved to ./test/output_$${TIME}.txt"$(COLOR_END)
