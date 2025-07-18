//! this file does blablabla

#include <stdint.h>

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"


/*int stat(int inode_num) {
    int ret = 0;
    
    if (!is_mounted())
        return ssfs_EMOUNT;
    
cleanup:
    return ret;
}*/

// Creates a file and, on success, returns the i-node number that identifies the
// file
int create() {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    if (!is_mounted())
        return ssfs_EMOUNT;
    
    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup;
    }
    superblock_t *sb = (superblock_t *)buffer;
    
    // Look for a free inode. Starting at index 1.
    // Foreach inode_block in the filesystem
    uint32_t inode_block_num = 0;
    for (uint32_t block_num = 1; block_num < 1 + sb->num_inode_blocks; block_num++) {
        ret = vdisk_read(disk_handle, block_num, buffer);
        if (ret != 0) {
            ret = vdisk_EACCESS;
            goto cleanup;
        }

        
        inodes_block_t *inodes_block = (inodes_block_t *)buffer;

        // Foreach inode
        for (int i = 0; i < 32; i++) {
            if (inodes_block[0][i].valid == 0) {

                inodes_block[0][i].valid = 1;

                ret = vdisk_write(disk_handle, block_num, buffer);
                if (ret != 0) {
                    ret = vdisk_EACCESS;
                    goto cleanup;
                }
                
                ret = vdisk_sync(disk_handle);
                if (ret != 0) {
                    ret = vdisk_EACCESS;
                    goto cleanup;
                }

                // Return the inode number
                return inode_block_num * 32 + i;
            }
        }
        inode_block_num++;
    }

cleanup:
    return ret;
}

//int delete(int inode_num) {}
