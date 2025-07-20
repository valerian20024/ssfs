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

    uint8_t *data = malloc(sizeof(50000));

    int inode = 1;

    mount(disk_name);

    int size = stat(inode);
    printf("size(%d) = %d\n", inode, size);

    int bytes = read(inode, data, 1000, 0);  // 10000 bytes won't work, but 1000 do?
    printf("succesfully read %d bytes\n", bytes);
    
    unmount();
}