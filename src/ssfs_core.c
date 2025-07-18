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

DISK* global_disk_handle = NULL;

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

    if (is_mounted()) {
        ret = ssfs_EMOUNT;
        goto cleanup;
    }
    
    // Turning on the virtual disk
    DISK disk;
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
    global_disk_handle = malloc(sizeof(DISK));
    if (global_disk_handle == NULL) {
        ret = ssfs_EDISKPTR;
        goto cleanup_disk;
    }
    *global_disk_handle = disk;

    // Allocating the block allocation bitmap
    allocated_blocks = (bool *)calloc(sb->num_blocks, sizeof(bool));
    if (allocated_blocks == NULL) {
        ret = ssfs_EALLOC;
        goto cleanup_global_disk_handle;
    }


    // todo :
    // ret = _mark_allocated_blocks 
    // that function is defined in this file, will use shared functions defined
    // in utils. 
    // Just give it ptr to bitmap and disk, it will firstt
    // mark the sb and the inodes blocks as used, then recursively
    // checks all the inodes's data and shit to know what DB is used.
    // Only output a single error : ssfs_EMARKING or idk
    // goto cleanup_allocated_blocks if failure
    // Look at how it's done in fs.c to redo the utils functions.




    // If everything went right, simply return.
    goto cleanup;

    // Else, we incrementaly free ressources.
cleanup_allocated_blocks:
    free(allocated_blocks);
    allocated_blocks = NULL;

cleanup_global_disk_handle:
    free(global_disk_handle);
    global_disk_handle = NULL;

cleanup_disk:
    vdisk_off(global_disk_handle);

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

    vdisk_off(global_disk_handle);
    free(global_disk_handle);
    global_disk_handle = NULL;
    free(allocated_blocks);
    allocated_blocks = NULL;

cleanup:
    if (ret != 0)
        fprintf(stderr, "Error when unmounting (code %d).\n", ret);
    return ret;
}


int _mark_allocated_blocks() {

}