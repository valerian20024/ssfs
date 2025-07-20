#include "ssfs_internal.h"
#include "fs.h"
#include <stdbool.h>
#include <stdlib.h>

void test1();
void test2();


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

    uint8_t * data = malloc(sizeof(2048));

    mount(disk_name);
    
    read(2, data, 2048, 0);
    
    unmount();
}