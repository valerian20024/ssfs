//! this file aims at defining helpers functions used in several
//! places in the code.

#include "vdisk.h"
#include "ssfs_internal.h"

// This function returns 0 is the disk is not mounted and 1 otherwise.
int is_mounted() {
    return global_disk_handle == NULL ? 0 : 1;
}

int is_inode_positive(int inode_num) {
    return inode_num > 0;
}

int is_inode_valid(int inode_num, int max_inode_num) {
    return is_inode_positive && inode_num <= max_inode_num;
}

// This functions 
int check_magic(DISK *disk) {
}