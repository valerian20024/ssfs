//! this file aims at defining helpers functions used in several
//! places in the code.


// The functions in this file often return 0 for false condition and 1 for true conditions. This allows to call them in the following way :
// if (is_magic_ok()) { ... } which will effectively execute if
// the magic number is good.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "vdisk.h"
#include "ssfs_internal.h"


const int SUPERBLOCK_SECTOR = 0;
const uint8_t MAGIC_NUMBER[16] = {
    0xf0, 0x55, 0x4c, 0x49,
    0x45, 0x47, 0x45, 0x49,
    0x4e, 0x46, 0x4f, 0x30,
    0x39, 0x34, 0x30, 0x0f 
};

bool *allocated_blocks;


// This function returns 0 is the disk is not mounted and 1 otherwise.
int is_mounted() {
    return global_disk_handle == NULL ? 0 : 1;
}

// This function returns 0 is the inode is negative or zero, 1 if it's strictly positive.
int is_inode_positive(int inode_num) {
    return inode_num > 0;
}

// This function returns 0 is the inode is not valid and 1 if it is. 
// Valid means to be strictly positive and not overflow the maximum number of inodes in the filesystem.
int is_inode_valid(int inode_num, int max_inode_num) {
    return is_inode_positive(inode_num) && inode_num <= max_inode_num;
}

// This function returns 0 if the magic numbers don't correspond. Return 1 if they correspond.
int is_magic_ok(uint8_t *number) {
    int ret = memcmp(number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
    return ret == 0 ? 1 : 0;
}