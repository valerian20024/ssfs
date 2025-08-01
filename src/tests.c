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
    fprintf(stdout, "%sStarting test1...%s\n", COLOR_YELLOW, COLOR_RESET);

    char *disk_name = "testdisk.img";
    int inodes = 200;

    fprintf(stdout, "%sFormatting%s %s with at least %d inodes\n", COLOR_BLUE, COLOR_RESET, disk_name, inodes);
    format(disk_name, inodes);

    fprintf(stdout, "%sMounting...%s\n", COLOR_BLUE, COLOR_RESET);
    mount(disk_name);
    
    srand((unsigned int)time(NULL));
    int files_num = (rand() % (inodes / 2 - 1)) + 1; // at least 1, at most inodes / 2 - 1
    int delete_files_num = (rand() % files_num) + 1; // at least 1, at most files_num

    fprintf(stdout, "%sCreating:%s %d files\n", COLOR_BLUE, COLOR_RESET, files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        printf("created file with inode %d\n", file);
    }

    fprintf(stdout, "%sStatistics:%s %d files\n", COLOR_BLUE, COLOR_RESET, files_num);
    for (int f = 0; f < files_num; f++) {
        int ret = stat(f);
        printf("stat(%d): %d bytes\n", f, ret);
    }
    
    fprintf(stdout, "%sDeleting:%s %d files\n", COLOR_BLUE, COLOR_RESET, delete_files_num);
    for (int f = 0; f < delete_files_num; f++) {
        int ret = delete(f);
        if (ret == 0)
            printf("Deleted file %d\n", f);
        else
            printf("%sError when deleting file %d%s\n", COLOR_RED, f, COLOR_RESET);
    }

    files_num = (rand() % (inodes / 2 - 1)) + 1;

    fprintf(stdout, "%sCreating:%s %d files\n", COLOR_BLUE, COLOR_RESET, files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        printf("Created file %d\n", file);
    }

    fprintf(stdout, "%sUnmounting ...%s\n", COLOR_BLUE, COLOR_RESET);
    unmount();

    fprintf(stdout, "\n");
}

// Reading some files with different lengths and offsets
void test2() {
    fprintf(stdout, "%sStarting test2...%s\n", COLOR_YELLOW, COLOR_RESET);

    int bytes_num = 10000; 
    print_info("Allocating ressources", "%d", bytes_num);
    uint8_t *data = malloc(sizeof(bytes_num));

    int inodes[]    = {1, 2, 3};
    int lens[]      = {10, 100, 1000};
    int offsets[]   = {0, 10, 100};

    int num_inodes  = sizeof(inodes) / sizeof(inodes[0]);
    int num_lens    = sizeof(lens) / sizeof(lens[0]);
    int num_offsets = sizeof(offsets) / sizeof(offsets[0]);

    char *disk_name = "disk_img.2";
    print_info("Mounting", "%s", disk_name);
    mount(disk_name);

    for (int i = 0; i < num_inodes; i++) {
        for (int l = 0; l < num_lens; l++) {
            for (int o = 0; o < num_offsets; o++) {
                int inode = inodes[i];
                int len = lens[l];
                int offset = offsets[o];

                fprintf(stdout, "%sReading...%s\n", COLOR_BLUE, COLOR_RESET);
                print_info("inode: ", "%d", inode);
                print_info("len: ", "%d", len);
                print_info("offset: ", "%d", offset);

                fprintf(stdout, "%sStatistics...%s\n", COLOR_BLUE, COLOR_RESET);
                int size = stat(inode);
                printf("size(%d) = %d\n", inode, size);

                int bytes = read(inode, data, len, offset);

                // In green when read >= 0, red when read negative bytes
                if (bytes >= 0)
                    print_success("Number of bytes successfully read", "%d", bytes);
                else
                    print_error("Error when reading", "%d", bytes);
                
                printf("%sContent of data:%s\n", COLOR_BLUE, COLOR_RESET);
                for (int i = 0; i < bytes; i++) {
                    printf("%02x ", data[i]);
                    if ((i + 1) % 16 == 0) printf("\n");
                    if ((i + 1) % 128 == 0) printf("\n");
                }
                printf("\n");
                
            }
        }
    }

    fprintf(stdout, "%sUnmounting ...%s\n", COLOR_BLUE, COLOR_RESET);
    unmount();
}


/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <blue>label:<white>formatted_message<reset>\n
 *
 * @param label The label to print (e.g., "Mounting").
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_info("Mounting", "%s", disk_name); where disk_name is a string.
 * 
 */
void print_info(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stdout, "%s%s:%s ", COLOR_BLUE, label, COLOR_WHITE);
    vfprintf(stdout, format, args);
    fprintf(stdout, "%s\n", COLOR_RESET);

    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <red>label:formatted_message<reset>\n
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

    fprintf(stdout, "%s%s: ", COLOR_RED, label);
    vfprintf(stdout, format, args);
    fprintf(stdout, "%s\n", COLOR_RESET);

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
 * @note Example: print_success("Succesfully done: ", "%d", return_code); where return_code is an integer.
 * 
 */
void print_success(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stdout, "%s%s:%s ", COLOR_GREEN, label, COLOR_WHITE);
    vfprintf(stdout, format, args);
    fprintf(stdout, "%s\n", COLOR_RESET);

    va_end(args);
}



