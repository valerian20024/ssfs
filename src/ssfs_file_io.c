//! This file does blablabla

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

int stat(int inode_num) {
    int ret = 0;
    if (!is_mounted())
        return ssfs_EMOUNT;

cleanup:
    return ret;
}
int read(int inode_num, uint8_t *data, int len, int offset) {}
int write(int inode_num, uint8_t *data, int len, int offset) {}