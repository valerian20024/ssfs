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


// format, mount, create 10 files, give stats, delete 5, create 10, unmount
void test1() {
    char *disk_name = "testdisk.img";
    format(disk_name, 100);

    mount(disk_name);
    
    for (int f = 0; f < 10; f++) {
        int file = create();
        printf("created file with inode %d\n", file);
    }

    for (int f = 0; f < 10; f++) {
        int ret = stat(f);
        printf("stat(%d): %d bytes\n", f, ret);
    }
    
    for (int f = 0; f < 5; f++) {
        delete(f);
        printf("deleted file %d\n", f);
    }

    for (int f = 0; f < 10; f++) {
        int file = create();
        printf("created file %d\n", file);
    }

    unmount();
}

void test2() {
    char *disk_name = "disk_img.2";

    uint8_t *data = malloc(sizeof(2120318));

    int inode = 1;
    int len = 2120318;
    int offset = 0;

    mount(disk_name);

    int size = stat(inode);
    printf("size(%d) = %d\n", inode, size);

    int bytes = read(inode, data, len, offset);
    printf("succesfully read %d bytes\n", bytes);
    
    /*printf("Content of data:\n");
    for (int i = 0; i < bytes; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
        if ((i + 1) % 128 == 0) printf("\n");
    }
    printf("\n");
    */
    unmount();
}
