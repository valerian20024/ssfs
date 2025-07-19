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
    superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    memcpy(sb.magic, MAGIC_NUMBER, 16);
    sb.num_blocks       = disk.size_in_sectors;
    sb.num_inode_blocks = inode_blocks;
    sb.block_size       = VDISK_SECTOR_SIZE;
    memcpy(buffer, &sb, sizeof(superblock_t));
    ret = vdisk_write(&disk, 0, buffer);
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
 * @brief Mounts a virtual disk for use.
 *
 * This function prepares a virtual disk, specified by its image file, for access
 * by the file system. It initializes internal data structures and makes the
 * file system ready for operations like creating, reading, and writing files.
 *
 * @param disk_name The file path of the disk image to mount, as a C-style string.
 *
 * @return 0 on success.
 * @return Negative integer (error codes) on failure.
 *
 * @note This implementation assumes and enforces that only a single volume
 * can be mounted at any given time. Subsequent calls to `mount` while another
 * volume is already mounted will result in an error.
 */
int mount(char *disk_name) {
    fprintf(stdout, "mount\n");
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    if (is_mounted()) {
        ret = ssfs_EMOUNT;
        goto cleanup_simple_error;
    }

    // Allocate the global disk pointer
    disk_handle = malloc(sizeof(DISK));
    if (disk_handle == NULL) {
        ret = ssfs_EDISKPTR;
        goto cleanup_simple_error;
    }
    
    // Turning on the virtual disk
    ret = vdisk_on(disk_name, disk_handle);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup_deallocate_disk_handle;
    }

    // Reading superblock
    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0)
        goto cleanup_shut_down_disk;
    superblock_t *sb = (superblock_t *)buffer;

    // Check magic number
    if (!is_magic_ok(sb->magic)) {
        ret = ssfs_EMAGIC;
        goto cleanup_shut_down_disk;
    }

    // Allocating the block allocation bitmap
    allocated_blocks_handle = (bool *)calloc(sb->num_blocks, sizeof(bool));
    if (allocated_blocks_handle == NULL) {
        ret = ssfs_EALLOC;
        goto cleanup_shut_down_disk;
    }

    ret = _initialize_allocated_blocks();
    if (ret != 0)
        goto cleanup_deallocated_blocks_handle;
    
    // If everything went right, simply return.
    goto cleanup_simple_error;

    // Else, we incrementaly free ressources.
cleanup_deallocated_blocks_handle:
    free(allocated_blocks_handle);
    allocated_blocks_handle = NULL;

cleanup_shut_down_disk:
    vdisk_off(disk_handle);

cleanup_deallocate_disk_handle:
    free(disk_handle);
    disk_handle = NULL;

cleanup_simple_error:
    if (ret != 0) {
        fprintf(stderr, "Error when mounting (code %d).\n", ret);
    }
    return ret;
}

/**
 * @brief Unmounts the currently mounted virtual disk volume.
 *
 * This function disengages the mounted virtual disk from the file system,
 * releasing associated resources and making it unavailable for further operations
 * until mounted again.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 *
 * @note This function will fail if no volume is currently mounted.
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

/**
 * @brief Initializes the block allocation bitmap based on existing file system usage.
 *
 * This internal procedure scan sthe file system to identify all blocks currently
 * in use (e.g., by the superblock, inodes, and existing data). It then updates
 * a global bitmap variable to reflect these used blocks.
 *
 * @return 0 on success.
 * @return Negative integer (error codes) on failure.
 *
 * @note This function is primarily used during the `mount()` operation to establish
 * the initial state of the bitmap.
 */

int _initialize_allocated_blocks() {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Read the superblock
    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) 
        return ret;
    superblock_t *sb = (superblock_t *)buffer;

    // Compute the number of system blocks and mark them.
    int system_blocks = 1 + sb->num_inode_blocks;
    for (int block_num = 0; block_num < system_blocks; block_num++) {
        allocate_block(block_num);
    }

    // Foreach inode block in the filesystem
    for (uint32_t block_num = 1; block_num < 1 + sb->num_inode_blocks; block_num++) {
        ret = vdisk_read(disk_handle, block_num, buffer);
        //! manage error must deallocate the direct blocks!
        inodes_block_t *ib = (inodes_block_t *)buffer;
        
        // For each inode in an inode block, if inode has been allocated
        for (int i = 0; i < 32; i++) {
            if (ib[0][i].valid) {
                // Allocate direct blocks
                for (int d = 0; d < 4; d++) {
                    fprintf(stdout, "d is %d\n", d);
                    if (ib[0][i].direct[d]) {
                        fprintf(stdout, "ib[0][i].direct[d] is true\n");
                        allocate_block(ib[0][i].direct[d]);
                    }
                }

                if (ib[0][i].indirect1)
                    allocate_indirect_block(ib[0][i].indirect1);

                if (ib[0][i].indirect2)
                    allocate_double_indirect_block(ib[0][i].indirect2);
            }
        }
    }

    return ret;
}
