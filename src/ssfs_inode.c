/*
 * ssfs_inode.c
 * ============
 * 
 * This file handles all operations on inodes.
 * That is, getting statistics about a file, creating and deleting a file
 * and all their local helper functions. 
 * 
 */

#include <stdint.h>
#include <string.h>

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

/**
 * @brief Retrieves the size of a file identified by its inode number.
 *
 * This function queries the file system to obtain details about a specific file,
 * identified by its unique inode number.
 *
 * @param inode_num The inode number of the file to retrieve information from.
 *
 * @return The file size in bytes on success.
 * @return Negative integer (error codes) on failure.
 *
 * @note The error codes are defined in `error.c`.
 */
int stat(int inode_num) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];
    
    if (!is_mounted())
        return ssfs_EMOUNT;
    
    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }
    superblock_t *sb = (superblock_t *)buffer;
    
    // Checking validity of the function parameter
    uint32_t total_inodes = sb->num_inode_blocks * 32;
    if (!is_inode_valid(inode_num, total_inodes)) {
        ret = ssfs_EALLOC;
        goto error_management;
    }

    // Determining which precise inode we are looking for
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num   = inode_num % 32;

    // Reading the sector where the inode is (skip the superblock)
    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }

    inodes_block_t *ib = (inodes_block_t *)buffer;
    inode_t *target_inode = &(*ib)[target_inode_num];

    if (target_inode->valid == 0) {
        ret = ssfs_EINODE;
        goto error_management;
    }

    return (int)target_inode->size;
    
error_management:
    fprintf(stderr, "Error when retrieving stats of file (code %d)\n", ret);
    return ret;
}

/**
 * @brief Creates a new file in the file system.
 *
 * This function allocates the necessary resources to create a new file,
 * which consists of assigning an inode.
 *
 * @return The inode number that identifies the newly created file on success.
 * @return Negative integer (error codes) on failure.
 *
 * @note Inode numbers start from zero included.
 */
int create() {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto error_management;
    }
    
    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }
    superblock_t *sb = (superblock_t *)buffer;
    
    // Look for a free inode. Starting at index 1.
    // Foreach inode_block in the filesystem
    uint32_t inode_block_num = 0;
    for (uint32_t block_num = 1; block_num < 1 + sb->num_inode_blocks; block_num++) {
        ret = vdisk_read(disk_handle, block_num, buffer);
        if (ret != 0) {
            ret = vdisk_EACCESS;
            goto error_management;
        }

        inodes_block_t *inodes_block = (inodes_block_t *)buffer;

        // Foreach inode
        for (int i = 0; i < 32; i++) {
            if (inodes_block[0][i].valid == 0) {
                inodes_block[0][i].valid = 1;

                ret = vdisk_write(disk_handle, block_num, buffer);
                if (ret != 0) {
                    ret = vdisk_EACCESS;
                    goto error_management;
                }
                
                ret = vdisk_sync(disk_handle);
                if (ret != 0) {
                    ret = vdisk_EACCESS;
                    goto error_management;
                }

                // Return the inode number
                return inode_block_num * 32 + i;
            }
        }
        inode_block_num++;
    }

    return ret;

error_management:
    fprintf(stderr, "Error when creating a new file (code %d)\n", ret);
    return ret;
}

/**
 * @brief Deletes a file from the file system.
 *
 * This function removes the file identified by `inode_num` from the file system.
 * This involves resetting the corresponding inode structure and freeing all the
 * blocks used by that file.
 *
 * @param inode_num The inode number of the file to be deleted.
 *
 * @return 0 on success.
 * @return Negative integer (error codes) on failure.
 *
 * @note As "SSFS is said to be safe because everything is always zero, 
 * unless other values are strictly needed", this function must rewrite zeros
 * on all deleted blocks.
 */
int delete(int inode_num) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto error_management;
    }

    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }
    superblock_t *sb = (superblock_t *)buffer;

    // Checking validity of the function parameter
    uint32_t total_inodes = sb->num_inode_blocks * 32;
    if (!is_inode_valid(inode_num, total_inodes)) {
        ret = ssfs_EALLOC;
        goto error_management;
    }

    // Determining which precise inode we are looking for
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num   = inode_num % 32;

    // Reading the sector where the inode is (skip the superblock)
    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }
    inodes_block_t *ib = (inodes_block_t *)buffer;

    inode_t * target_inode = &ib[0][target_inode_num]; 
    if (!target_inode->valid) {
        ret = ssfs_EINODE;
        goto error_management;
    }

    // Mark the inode as free
    // todo test this !!
    target_inode->valid = 0;
    target_inode->size = 0;
    memset(target_inode->direct, 0, sizeof(target_inode->direct));
    target_inode->indirect1 = 0;
    target_inode->indirect2 = 0;
    vdisk_write(disk_handle, 1 + target_inode_block, buffer);

    // Freeing blocks
    for (int d = 0; d < 4; d++) {
        if (target_inode->direct[d]) {
            deallocate_block(target_inode->direct[d]);
        }
    }

    if (target_inode->indirect1) {
        deallocate_indirect_block(target_inode->indirect1);
    }

    if (target_inode->indirect2) {
        deallocate_double_indirect_block(target_inode->indirect2);
    }

    return ret;

error_management:
    fprintf(stderr, "Error when deleting a file (code %d)\n", ret);
    return ret;
}

