//! this file does blablabla

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

DISK* global_disk_handle = NULL;

int my_var = 5;

int format(char *disk_name, int inodes) {
    if (is_mounted())
        return ssfs_EMOUNT;
}

int mount(char *disk_name) {}
int unmount() {
    my_var = 7;
}