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

const char *COLOR_RESET = "\033[0m";
const char *COLOR_RED = "\033[1;31m";
const char *COLOR_GREEN = "\033[1;32m";
const char *COLOR_YELLOW = "\033[1;33m";
const char *COLOR_BLUE = "\033[1;34m";
const char *COLOR_MAGENTA = "\033[1;35m";
const char *COLOR_CYAN = "\033[1;36m";
const char *COLOR_WHITE = "\033[1;37m";

// To copy for output formatting:
// fprintf(stdout, "%s %s\n", COLOR_BLUE, COLOR_RESET);

// format, mount, create 10 files, give stats, delete 5, create 10, unmount
void test1() {
    fprintf(stdout, "%sStarting test1...%s\n", COLOR_GREEN, COLOR_RESET);

    char *disk_name = "testdisk.img";
    int inodes = 100;

    fprintf(stdout, "%sFormatting%s %s with at least %d inodes\n", COLOR_BLUE, COLOR_RESET, disk_name, inodes);
    format(disk_name, inodes);

    fprintf(stdout, "%sMounting...%s\n", COLOR_BLUE, COLOR_RESET);
    mount(disk_name);
    
    int files_num = 10;
    int delete_files_num = 5;

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
        delete(f);
        printf("Deleted file %d\n", f);
    }

    fprintf(stdout, "%sCreating:%s %d files\n", COLOR_BLUE, COLOR_RESET, files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        printf("Created file %d\n", file);
    }

    fprintf(stdout, "%sUnmounting ...%s\n", COLOR_BLUE, COLOR_RESET);
    unmount();
}

// Reading a file
void test2() {
    fprintf(stdout, "%sStarting test2...%s\n", COLOR_GREEN, COLOR_RESET);
    
    fprintf(stdout, "%sAllocating ressources:%s\n", COLOR_BLUE, COLOR_RESET);

    int bytes_num = 2120318; 
    char *disk_name = "disk_img.2";
    uint8_t *data = malloc(sizeof(2120318));

    int inode = 1;
    int len = 2120318;
    int offset = 0;

    fprintf(stdout, "Max number of bytes for data: %d\n", bytes_num);
    fprintf(stdout, "len: %d\n", len);
    fprintf(stdout, "offset: %d\n", len);

    fprintf(stdout, "%sMounting:%s %s\n", COLOR_BLUE, COLOR_RESET, disk_name);
    mount(disk_name);

    fprintf(stdout, "%sStats to find the size:%s\n", COLOR_BLUE, COLOR_RESET);
    int size = stat(inode);
    printf("size(%d) = %d\n", inode, size);

    int bytes = read(inode, data, len, offset);
    
    // In green when read >= 0, red when read negative bytes
    if (bytes >= 0)
        printf("%sSuccesfully read %d bytes%s\n", COLOR_GREEN, bytes, COLOR_RESET);
    else
        printf("%sError : %d%s\n", COLOR_RED, bytes, COLOR_RESET);
    
    printf("%sContent of data:%s\n", COLOR_BLUE, COLOR_RESET);
    for (int i = 0; i < bytes; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
        if ((i + 1) % 128 == 0) printf("\n");
    }
    printf("\n");
    
    fprintf(stdout, "%sUnmounting ...%s\n", COLOR_BLUE, COLOR_RESET);
    unmount();
}
