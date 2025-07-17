//! this file does blablabla
#include <stdint.h>
#include <string.h>

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

DISK* global_disk_handle = NULL;

int format(char *disk_name, int inodes) {
    if (is_mounted())
        return ssfs_EMOUNT;

    if (!is_inode_positive(inodes))
        inodes = 1;
    
    // Turning on the virtual disk
    DISK disk;
    if (vdisk_on(disk_name, &disk) != 0)
        return vdisk_EACCESS;
    
    // Calculate how many 32-inodes blocks are needed.
    // Need at least 1 superblock + inode blocks + 1 data block
    uint32_t inode_blocks = (inodes + 31) / 32;
    if (disk.size_in_sectors < inode_blocks + 2) {
        vdisk_off(&disk);
        return ssfs_ENOSPACE;
    }
    
    // Clear all the sectors on disk
    uint8_t buffer[VDISK_SECTOR_SIZE];
    memset(buffer, 0, VDISK_SECTOR_SIZE);
    for (uint32_t sector = 0; sector < disk.size_in_sectors; sector++) {
        if (vdisk_write(&disk, sector, buffer) != 0) {
            vdisk_off(&disk);
            return vdisk_EACCESS;
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
    if (vdisk_write(&disk, SUPERBLOCK_SECTOR, buffer) != 0) {
        vdisk_off(&disk);
        return ssfs_ESBINIT;
    }
}

int mount(char *disk_name) {}
int unmount() {}