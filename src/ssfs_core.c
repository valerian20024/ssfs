/*
!This file does blabla


Functions often use a `ret` variable and goto statements to handle error codes
gracefully. In such a case, one need to declare any "variable" size object 
before any goto statement, otherwise compiler will complain. This is the case
for `buffer` which appears often in functions.

*/


#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

DISK* disk_handle = NULL;
bool* allocated_blocks_handle = NULL;

/**
 * @brief Formats a disk with the Simple and Secure File System (SSFS).
 *
 * This function initializes a disk image file with a new SSFS file system. It creates the
 * superblock in the first sector of the disk image.
 *
 * @param disk_name The file path of the disk image to format as a C-style string.
 * @param inodes The desired number of inodes to create in the file system. If this
 * value is 0 or negative, it will default to 1.
 *
 * @return 0 on success.
 * @return Negative integer (error codes) on failure. See errors.h.
 *
 * @note This is a destructive operation. Any existing data on the disk image
 * will be erased. The function assumes the disk is not currently mounted.
 */
int format(char *disk_name, int inodes) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    if (is_mounted())
        return ssfs_EMOUNT;

    if (!is_inode_positive(inodes))
        inodes = 1;

    // Turning on the virtual disk
    DISK disk;
    ret = vdisk_on(disk_name, &disk);
    if (ret != 0) 
        goto cleanup; 
    
    // Calculate how many 32-inodes blocks are needed.
    // Need at least 1 superblock + inode blocks + 1 data block
    uint32_t inode_blocks = (inodes + 31) / 32;
    if (disk.size_in_sectors < inode_blocks + 2) {
        ret = ssfs_ENOSPACE;
        goto cleanup;
    }
    
    // Clear all the sectors on disk
    memset(buffer, 0, VDISK_SECTOR_SIZE);
    for (uint32_t sector = 0; sector < disk.size_in_sectors; sector++) {
        ret = vdisk_write(&disk, sector, buffer);
        if (ret != 0) {
            ret = vdisk_EACCESS;
            goto cleanup;
        }
    }

    // Initialize and fill the superblock
    struct superblock sb;
    memset(&sb, 0, sizeof(sb));
    memcpy(sb.magic, MAGIC_NUMBER, 16);
    sb.num_blocks       = disk.size_in_sectors;
    sb.num_inode_blocks = inode_blocks;
    sb.block_size       = VDISK_SECTOR_SIZE;
    memcpy(buffer, &sb, sizeof(struct superblock));
    ret = vdisk_write(&disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0)
        goto cleanup;

    // Terminate connection with virtual disk
    ret = vdisk_sync(&disk);
cleanup:
    vdisk_off(&disk);
    if (ret != 0) 
        fprintf(stderr, "Error when formatting (code %d).\n", ret);
    return ret;
}

/**
 * 
 */
int mount(char *disk_name) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];
    DISK disk;
    bool *allocated_blocks;

    if (is_mounted()) {
        ret = ssfs_EMOUNT;
        goto cleanup;
    }
    
    // Turning on the virtual disk
    ret = vdisk_on(disk_name, &disk);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup;
    }

    // Reading superblock
    ret = vdisk_read(&disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0)
        goto cleanup_disk;
    struct superblock *sb = (struct superblock *)buffer;

    // Check magic number
    if (!is_magic_ok(sb->magic)) {
        ret = ssfs_EMAGIC;
        goto cleanup_disk;
    }

    // Allocate the global disk pointer
    disk_handle = malloc(sizeof(DISK));
    if (disk_handle == NULL) {
        ret = ssfs_EDISKPTR;
        goto cleanup_disk;
    }
    *disk_handle = disk;

    // Allocating the block allocation bitmap
    allocated_blocks = (bool *)calloc(sb->num_blocks, sizeof(bool));
    if (allocated_blocks == NULL) {
        ret = ssfs_EALLOC;
        goto cleanup_disk_handle;
    }
    *allocated_blocks_handle = allocated_blocks; 


    ret = _initialize_allocated_blocks(allocated_blocks_handle);
    if (ret != 0)
        goto cleanup_allocated_blocks_handle;
    
    // If everything went right, simply return.
    goto cleanup;

    // Else, we incrementaly free ressources.
cleanup_allocated_blocks_handle:
    free(allocated_blocks_handle);
    allocated_blocks_handle = NULL;

cleanup_disk_handle:
    free(disk_handle);
    disk_handle = NULL;

cleanup_disk:
    vdisk_off(disk_handle);

cleanup:
    if (ret != 0) {
        fprintf(stderr, "Error when mounting (code %d).\n", ret);
    }
    return ret;
}

/**
 * 
 */
int unmount() {
    int ret = 0;

    if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto cleanup;
    }

    vdisk_off(disk_handle);
    free(disk_handle);
    disk_handle = NULL;
    free(allocated_blocks_handle);
    allocated_blocks_handle = NULL;

cleanup:
    if (ret != 0)
        fprintf(stderr, "Error when unmounting (code %d).\n", ret);
    return ret;
}


int _initialize_allocated_blocks() {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Read the superblock
    ret = vdisk_read(disk_handle, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) 
        return ret;
    struct superblock *sb = (struct superblock *)buffer;

    // Compute the number of system blocks and mark them.
    int system_blocks = 1 + sb->num_inode_blocks;
    for (int block = 0; block < system_blocks; block++) {
        allocate_block(block);
    }

    // Foreach inode block in the filesystem
    for (uint32_t block = 1; block < sb->num_inode_blocks; block++) {
        ret = vdisk_read(disk_handle, block, buffer);
        //! manage error must deallocate the direct blocks!
        inodes_block_t *ib = (inodes_block_t *)buffer;
        
        // For each inode in an inode block
        for (int i = 0; i < 32; i++) {
            if (!ib[i]->valid)
                continue;

            // Allocate direct blocks
            for (int d = 0; d < 4; d++) {
                if (ib[i]->direct[d])
                    allocate_block(ib[i]->direct[d]);
            }

            if (ib[i]->indirect1)
                _allocate_indirect_block(ib[i]->indirect1);

            if (ib[i]->indirect2)
                _allocate_double_indirect_block(ib[i]->indirect2);            
        }
    }

    return ret;
}

// This function will allocate an indirect block as well as
// all the data blocks it points to.
// Returns 0 on success, error code if not.
int _allocate_indirect_block(uint32_t indirect_block) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    ret = vdisk_read(disk_handle, indirect_block, buffer);
    if (ret != 0)
        goto cleanup;

    uint32_t *data_blocks = (uint32_t *)buffer;
    for (int db = 0; db < 256; db++) {
        if (data_blocks[db])
            allocate_block(data_blocks[db]);
    }
    allocate_block(indirect_block);

cleanup:
    return ret;
}


// This function will allocate a double indirect block as well as
// all the indirect block it points to and all the data blocks they point to.
// Returns 0 on success, error code if not
int _allocate_double_indirect_block(uint32_t double_indirect_block) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];  // storing 2-indirect block

    ret = vdisk_read(disk_handle, double_indirect_block, buffer);
    if (ret != 0)
        goto cleanup;

    uint32_t *indirect_ptrs = (uint32_t *)buffer;
    for (int ip = 0; ip < 256; ip++) {
        if (indirect_ptrs[ip] == 0)
            continue;

        _allocate_indirect_block(indirect_ptrs[ip]);
    }

    allocate_block(double_indirect_block);

cleanup:
    return ret;
}