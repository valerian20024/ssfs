#include "ssfs_internal.h"
#include "fs.h"
#include <stdbool.h>

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(void) {

    char *disk_name = "testdisk.img";
    format(disk_name, 20);

    mount(disk_name);
    
    for (int f = 0; f < 10; f++) {
        int file = create();
        printf("created file with inode %d\n", file);
    }
    
    unmount();
}
