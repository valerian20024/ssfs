/*
 * tests.c
 * =======
 * 
 * This file only serves testing purposes.
 * It defines functions for different tests.
 *
 */


#include "ssfs_internal.h"
#include "fs.h"
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

const char *COLOR_RESET     = "\033[0m";
const char *COLOR_RED       = "\033[1;31m";
const char *COLOR_GREEN     = "\033[1;32m";
const char *COLOR_YELLOW    = "\033[1;33m";
const char *COLOR_BLUE      = "\033[1;34m";
const char *COLOR_MAGENTA   = "\033[1;35m";
const char *COLOR_CYAN      = "\033[1;36m";
const char *COLOR_WHITE     = "\033[1;37m";

// format, mount, create, stats, delete, create, unmount
void test1() {
    print_warning("Starting test1...", NULL);

    char *disk_name = "testdisk.img";
    int inodes = 200;

    print_info("Formatting", "%s", disk_name);
    print_info("Number of inodes", "%d", inodes);
    format(disk_name, inodes);

    print_info("Mounting...", NULL);
    mount(disk_name);
    
    srand((unsigned int)time(NULL));
    int files_num = (rand() % (inodes / 2 - 1)) + 1; // at least 1, at most inodes / 2 - 1
    int delete_files_num = (rand() % files_num) + 1; // at least 1, at most files_num

    print_info("Number of file to be created", "%d", files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        if (file >= 0)
            printf("Created file with inode %d\n", file);
        else
            print_error("Error when creating file: ", "%d", file);
    }

    print_info("Number of statistics", "%d", files_num);
    for (int f = 0; f < files_num; f++) {
        int ret = stat(f);
        if (ret >= 0)
            printf("Size(%d) -> %d bytes\n", f, ret);
        else
            print_error("Statistics error", "%d", ret);
    }
    
    print_info("Number of files to be deleted", "%d", delete_files_num);
    for (int f = 0; f < delete_files_num; f++) {
        int ret = delete(f);
        if (ret == 0)
            printf("Deleted file number: %d\n", f);
        else
            print_error("Error when deleting file number", "%d", f);
    }

    files_num = (rand() % (inodes / 2 - 1)) + 1;

    print_info("Number of file to be created", "%d", files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        if (file >= 0)
            printf("Created file with inode %d\n", file);
        else
            print_error("Error when creating file: ", "%d", file);
    }

    print_info("Unmounting...", NULL);
    unmount();
}

// Reading some files with different lengths and offsets
void test2() {
    print_warning("Starting test2...", NULL);

    int bytes_num = 2120318; 
    print_info("Allocating ressources", "%d", bytes_num);
    uint8_t *data = malloc(bytes_num);

    int inodes[]    = {1};
    int lens[]      = {2120318};
    int offsets[]   = {0};

    int num_inodes  = sizeof(inodes)    / sizeof(inodes[0]);
    int num_lens    = sizeof(lens)      / sizeof(lens[0]);
    int num_offsets = sizeof(offsets)   / sizeof(offsets[0]);

    char *disk_name = "disk_img.2";
    print_info("Mounting", "%s", disk_name);
    mount(disk_name);

    for (int i = 0; i < num_inodes; i++) {
        for (int l = 0; l < num_lens; l++) {
            for (int o = 0; o < num_offsets; o++) {
                int inode = inodes[i];
                int len = lens[l];
                int offset = offsets[o];

                print_info("Reading parameters", NULL);
                print_info("inode: ", "%d", inode);
                print_info("len: ", "%d", len);
                print_info("offset: ", "%d", offset);

                print_info("Statistics... ", NULL);
                int size = stat(inode);
                if (size >= 0)
                    printf("size(%d) = %d\n", inode, size);
                else
                    print_error("Error when reading", "%d", size);

                int bytes = read(inode, data, len, offset);
                if (bytes >= 0)
                    print_success("Number of bytes successfully read", "%d", bytes);
                else
                    print_error("Error when reading", "%d", bytes);
                
                print_info("Data content", NULL);
                for (int i = 0; i < bytes; i++) {
                    printf("%02x", data[i]);
                    if ((i + 1) % 16 == 0) printf("\n");
                    if ((i + 1) % 128 == 0) printf("\n");
                }
                printf("\n");
                
            }
        }
    }

    print_info("Unmounting...", NULL);
    free(data);
    unmount();
}

void test3() {
    print_warning("Starting test3...", NULL);

    int bytes_num = 10000;
    print_info("Allocating resources", "%d", bytes_num);
    uint8_t *data = malloc(bytes_num);
    if (!data) {
        print_error("Memory allocation failed", NULL);
        return;
    }

    // Fill data with a pattern
    for (int i = 0; i < bytes_num; i++) {
        data[i] = (uint8_t)(i % 256);
    }

    int inode = 0;
    int lens[] = {10, 100, 1000, 4096};
    int offsets[] = {0, 5, 50, 100};
    int num_lens = sizeof(lens) / sizeof(lens[0]);
    int num_offsets = sizeof(offsets) / sizeof(offsets[0]);

    char *disk_name = "disk_img.3";
    print_info("Mounting", "%s", disk_name);
    mount(disk_name);

    // Create a file to write to
    inode = create();
    if (inode < 0) {
        print_error("Failed to create file", "%d", inode);
        free(data);
        unmount();
        return;
    }
    print_success("Created file with inode", "%d", inode);

    for (int l = 0; l < num_lens; l++) {
        for (int o = 0; o < num_offsets; o++) {
            int len = lens[l];
            int offset = offsets[o];

            print_info("Writing parameters", NULL);
            print_info("inode: ", "%d", inode);
            print_info("len: ", "%d", len);
            print_info("offset: ", "%d", offset);

            int bytes = write(inode, data, len, offset);
            if (bytes >= 0)
                print_success("Number of bytes successfully written", "%d", bytes);
            else
                print_error("Error when writing", "%d", bytes);

            // Optionally, read back and verify
            uint8_t *verify = malloc(len);
            if (verify) {
                int read_bytes = read(inode, verify, len, offset);
                if (read_bytes == len && memcmp(data, verify, len) == 0) {
                    print_success("Verification passed", NULL);
                } else {
                    print_error("Verification failed", NULL);
                }
                free(verify);
            }
        }
    }

    print_info("Unmounting...", NULL);
    free(data);
    unmount();
}


/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <blue>label:<white>formatted_message<reset>\n
 * If format is NULL or empty, prints just the label in blue.
 *
 * @param label The label to print (e.g., "Mounting").
 * @param format The format string for the message (can be NULL).
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_info("Mounting", "%s", disk_name); where disk_name is a string.
 * @note Example: print_info("A simple label...", NULL);
 * 
 */
void print_info(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_BLUE, label, format, args);
    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <green>label:formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_error("Error", "%d", error_code); where error_code is an integer.
 * 
 */
void print_error(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_RED, label, format, args);
    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <green>label:formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_success("Success", "%d", return_code); where return_code is an integer.
 * 
 */
void print_success(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_GREEN, label, format, args);
    va_end(args);
}


/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <yellow>label:formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_warning("Warning", "%d", return_code); where return_code is an integer.
 * 
 */
void print_warning(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_YELLOW, label, format, args);
    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <color>label:<white>formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Typically called by higher functions such as print_info.
 * 
 */
void pretty_print(const char* color, const char *label, const char *format, va_list args) {
    if (format == NULL || format[0] == '\0') {
        fprintf(stdout, "%s%s%s\n", color, label, COLOR_RESET);
    } else {
        fprintf(stdout, "%s%s:%s ", color, label, COLOR_WHITE);
        vfprintf(stdout, format, args);
        fprintf(stdout, "%s\n", COLOR_RESET);
    }
}






