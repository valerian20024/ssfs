//! this file does blablabla
#include <stdint.h>
#include <string.h>

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
    if (is_mounted())
        return ssfs_EMOUNT;

    if (!is_inode_positive(inodes))
        inodes = 1;

    // Variable used to return error codes after cleanup
    int ret;
    // Initializing the buffer before any goto statement.
    uint8_t buffer[VDISK_SECTOR_SIZE];

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
    return ret;
}

/**
 * 
 */
int mount(char *disk_name) {
    if (is_mounted())
        return ssfs_EMOUNT;
    
    // Turning on the virtual disk
    DISK disk;
    if (vdisk_on(disk_name, &disk) != 0)
        return vdisk_EACCESS;

}

/**
 * 
 */
int unmount() {

}