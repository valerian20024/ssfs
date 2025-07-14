#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "fs.h"

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s name_of_the_function [arguments...]\n", argv[0]);
        return 1;
    }

    // For custom scripts to test the program
    if (strcmp(argv[1], "script") == 0) {
        int inodes = 34;
        format("disk_img.2", inodes);
        mount("disk_img.2");

        for (int i = 0; i < inodes + 40; i++) {
            create();
        }

        delete(5);
        


        unmount();

        fprintf(stdout, "Script executed.\n");
        return 0;
    }

    if (strcmp(argv[1], "format") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s format <disk_name> <inodes>\n", argv[0]);
            return 1;
        }
        char *disk_name = argv[2];
        int inodes = atoi(argv[3]);
        return format(disk_name, inodes);
    } else if (strcmp(argv[1], "stat") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s stat <inode_num>\n", argv[0]);
            return 1;
        }
        int inode_num = atoi(argv[2]);
        return stat(inode_num);
    } else if (strcmp(argv[1], "mount") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s mount <disk_name>\n", argv[0]);
            return 1;
        }
        char *disk_name = argv[2];
        return mount(disk_name);
    } else if (strcmp(argv[1], "unmount") == 0) {
        if (argc != 2) {
            fprintf(stderr, "Usage: %s unmount\n", argv[0]);
            return 1;
        }
        return unmount();
    } else if (strcmp(argv[1], "create") == 0) {
        if (argc != 2) {
            fprintf(stderr, "Usage: %s create\n", argv[0]);
            return 1;
        }
        return create();
    } else if (strcmp(argv[1], "delete") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s delete <inode_num>\n", argv[0]);
            return 1;
        }
        int inode_num = atoi(argv[2]);
        return delete(inode_num);
    } else if (strcmp(argv[1], "read") == 0) {
        if (argc != 6) {
            fprintf(stderr, "Usage: %s read <inode_num> <data> <len> <offset>\n", argv[0]);
            return 1;
        }
        int inode_num = atoi(argv[2]);
        uint8_t *data = (uint8_t *)argv[3];
        int len = atoi(argv[4]);
        int offset = atoi(argv[5]);
        return read(inode_num, data, len, offset);
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc != 6) {
            fprintf(stderr, "Usage: %s write <inode_num> <data> <len> <offset>\n", argv[0]);
            return 1;
        }
        int inode_num = atoi(argv[2]);
        uint8_t *data = (uint8_t *)argv[3];
        int len = atoi(argv[4]);
        int offset = atoi(argv[5]);
        return write(inode_num, data, len, offset);
    } else {
        fprintf(stderr, "Unknown function: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
