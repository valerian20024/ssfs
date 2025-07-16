#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/error.h"
#include "../include/vdisk.h"
#include "../include/helpers.h"

extern const int VDISK_SECTOR_SIZE;
extern const int INODES_PER_BLOCK;
extern const int SUPERBLOCK_SECTOR;
extern const unsigned char MAGIC_NUMBER[16];

#define INODES_PER_BLOCK 32

// Global disk handle
static DISK * global_disk = NULL;

// bitmap for storing what data blocks are used
static uint8_t *block_bitmap = NULL;


/*
Function to format the disk with SSFS 
*/
int format(char *disk_name, int inodes) {
    if (inodes <= 0) inodes = 1;

    // Prevent formatting if a disk is already mounted
    if (global_disk != NULL) {
        fprintf(stderr, "Disk is already mounted. Unmount it before formatting.\n");
        return vdisk_EACCESS;
    }

    // Try turning on the virtual disk
    DISK disk;
    int ret = vdisk_on(disk_name, &disk);
    if (ret != 0) return vdisk_EACCESS;

    // Calculate how many inode blocks are needed
    uint32_t inode_blocks = (inodes + 31) / 32; // 32 inodes per block

    // Minimum requirement: 1 superblock + inode blocks + at least 1 data block
    if (disk.size_in_sectors < inode_blocks + 2) {
        vdisk_off(&disk);
        return vdisk_ENOSPACE;
    }

    // Initialize and fill the superblock
    struct superblock sb;
    memset(&sb, 0, sizeof(sb)); // Clean slate
    memcpy(sb.magic, MAGIC_NUMBER, 16);
    sb.num_blocks = disk.size_in_sectors - 1; // Exclude superblock
    sb.num_inode_blocks = inode_blocks;
    sb.block_size = VDISK_SECTOR_SIZE;

    // Clear all sectors
    uint8_t buffer[VDISK_SECTOR_SIZE];
    memset(buffer, 0, VDISK_SECTOR_SIZE);

    for (uint32_t i = 0; i < disk.size_in_sectors; i++) {
        ret = vdisk_write(&disk, i, buffer);
        if (ret != 0) {
            vdisk_off(&disk);
            return ret;
        }
    }

    // Write the superblock to sector 0
    memcpy(buffer, &sb, sizeof(struct superblock));
    ret = vdisk_write(&disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) {
        vdisk_off(&disk);
        return ret;
    }

    // Sync and close the disk
    ret = vdisk_sync(&disk);
    vdisk_off(&disk);

    if (ret != 0) return ret;

    fprintf(stdout, "Disk formatted successfully.\n");
    return 0;
}

int stat(int inode_num) {
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return vdisk_ENODISK;
    }

    // Read superblock to know how many inodes the disk has
    uint8_t sb_buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(global_disk, SUPERBLOCK_SECTOR, sb_buffer);
    if (ret != 0) return ret;

    struct superblock *sb = (struct superblock *)sb_buffer;
    uint32_t total_inodes = sb->num_inode_blocks * INODES_PER_BLOCK;

    if (inode_num < 0 || (uint32_t)inode_num >= total_inodes) {
        fprintf(stderr, "Invalid inode number.\n");
        return -1;
    }

    // Déterminer le bloc et l'offset de l'inode
    int block = inode_num / INODES_PER_BLOCK;
    int offset = inode_num % INODES_PER_BLOCK;

    uint8_t buffer[VDISK_SECTOR_SIZE];
    ret = vdisk_read(global_disk, 1 + block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to read inode block %d: %d\n", 1 + block, ret);
        return ret;
    }

    struct inode *inodes = (struct inode *)buffer;
    struct inode *target_inode = &inodes[offset];

    if (target_inode->valid == 0) {
        fprintf(stderr, "Inode %d is not in use.\n", inode_num);
        return -1;
    }

    return (int)target_inode->size;
}

int mount(char *disk_name) {
    // Cannot mount if a disk is already mounted
    if (global_disk != NULL) {
        fprintf(stderr, "Disk is already mounted. Unmount it before mounting a new one.\n");
        return vdisk_EACCESS;
    }

    // Activate the virtual disk
    DISK disk;
    int ret = vdisk_on(disk_name, &disk);
    if (ret != 0) return vdisk_EACCESS;

    // Read the superblock
    uint8_t buffer[VDISK_SECTOR_SIZE];
    ret = vdisk_read(&disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) {
        vdisk_off(&disk);
        return ret;
    }

    struct superblock *sb = (struct superblock *)buffer;
    // Validate the magic number
    if (memcmp(sb->magic, MAGIC_NUMBER, 16) != 0) {
        vdisk_off(&disk);
        return vdisk_EINVALID;
    }

    // Allocate and store the global disk pointer
    global_disk = malloc(sizeof(DISK));
    if (global_disk == NULL) {
        vdisk_off(&disk);
        return -1;
    }
    *global_disk = disk;

    // Initialize global parameters
    total_blocks = sb->num_blocks + 1; // +1 to include the superblock itself
    data_start_block = 1 + sb->num_inode_blocks;

    // Allocate memory for the block bitmap
    block_bitmap = (uint8_t *)calloc(total_blocks, 1);
    if (block_bitmap == NULL) {
        free(global_disk);
        vdisk_off(global_disk);
        global_disk = NULL;
        return -1;
    }

    // Mark system blocks (superblock + inode blocks) as used
    for (int i = 0; i < data_start_block; i++) {
        mark_block_used(block_bitmap, i);
    }

    // Read all inode blocks and mark used data blocks
    for (uint32_t block = 0; block < sb->num_inode_blocks; block++) {
        ret = vdisk_read(global_disk, 1 + block, buffer);
        if (ret != 0) goto error_cleanup;

        struct inode *inodes = (struct inode *)buffer;
        for (int i = 0; i < INODES_PER_BLOCK; i++) {
            if (!inodes[i].valid) continue;

            // Mark direct blocks
            for (int j = 0; j < 4; j++) {
                if (inodes[i].direct[j]) {
                    mark_block_used(block_bitmap, inodes[i].direct[j]);
                }
            }

            // Mark single indirect block and its children
            if (inodes[i].indirect1) {
                mark_block_used(block_bitmap, inodes[i].indirect1);

                ret = vdisk_read(global_disk, inodes[i].indirect1, buffer);
                if (ret != 0) goto error_cleanup;

                uint32_t *ptrs = (uint32_t *)buffer;
                for (int j = 0; j < 256; j++) {
                    if (ptrs[j]) {
                        mark_block_used(block_bitmap, ptrs[j]);
                    }
                }
            }

            // Mark double indirect block and its children
            if (inodes[i].indirect2) {
                mark_block_used(block_bitmap, inodes[i].indirect2);

                uint8_t buffer2[VDISK_SECTOR_SIZE];
                ret = vdisk_read(global_disk, inodes[i].indirect2, buffer2);
                if (ret != 0) goto error_cleanup;

                uint32_t *ptr1 = (uint32_t *)buffer2;
                for (int j = 0; j < 256; j++) {
                    if (ptr1[j] == 0) continue;

                    mark_block_used(block_bitmap, ptr1[j]);

                    ret = vdisk_read(global_disk, ptr1[j], buffer);
                    if (ret != 0) goto error_cleanup;

                    uint32_t *ptr2 = (uint32_t *)buffer;
                    for (int k = 0; k < 256; k++) {
                        if (ptr2[k]) {
                            mark_block_used(block_bitmap, ptr2[k]);
                        }
                    }
                }
            }
        }
    }

    printf("Disk '%s' mounted successfully.\n", disk_name);
    return 0;

error_cleanup:
    // Clean up all allocated resources on error
    if (block_bitmap) {
        free(block_bitmap);
        block_bitmap = NULL;
    }
    if (global_disk) {
        vdisk_off(global_disk);
        free(global_disk);
        global_disk = NULL;
    }
    return ret;
}


int unmount() {
    // Fails if no disk is mounted
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return vdisk_ENODISK;
    }

    // Properly close the virtual disk
    vdisk_off(global_disk);

    // Clean up global state
    free(global_disk);
    global_disk = NULL;

    free(block_bitmap);
    block_bitmap = NULL;

    return 0;
}


int create() {
    // Check if disk is mounted
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return vdisk_ENODISK;
    }

    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Read the superblock to determine the number of inode blocks
    int ret = vdisk_read(global_disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) return ret;

    struct superblock *sb = (struct superblock *)buffer;
    uint32_t inode_blocks = sb->num_inode_blocks;

    // Look for a free inode
    for (uint32_t i = 0; i < inode_blocks; i++) {
        ret = vdisk_read(global_disk, SUPERBLOCK_SECTOR + 1 + i, buffer);
        if (ret != 0) return ret;

        struct inode_block *ib = (struct inode_block *)buffer;

        for (int j = 0; j < 32; j++) {
            if (ib->inodes[j].valid == 0) {
                // Free inode found
                ib->inodes[j].valid = 1;

                // Write back the inode block
                ret = vdisk_write(global_disk, SUPERBLOCK_SECTOR + 1 + i, buffer);
                if (ret != 0) return ret;

                ret = vdisk_sync(global_disk);
                if (ret != 0) return ret;

                // Return the inode number
                return i * 32 + j;
            }
        }
    }

    // No available inode found
    fprintf(stderr, "No available inode found.\n");
    return vdisk_ENOSPACE;
}



/*
This function is responsible for deleting a file, hence resetting the inode
and freeing the blocks it points to.
*/
int delete(int inode_num) {
    if (!global_disk) return vdisk_ENODISK;

    uint8_t buffer[VDISK_SECTOR_SIZE];
    struct superblock sb;
    if (vdisk_read(global_disk, 0, buffer) != 0) return -1;
    memcpy(&sb, buffer, sizeof(struct superblock));

    int inodes_per_block = VDISK_SECTOR_SIZE / sizeof(struct inode);
    int total_inodes = sb.num_inode_blocks * inodes_per_block;
    if (inode_num < 0 || inode_num >= total_inodes) return -1;

    int block_index = inode_num / inodes_per_block;
    int inode_index = inode_num % inodes_per_block;

    int inode_block_num = 1 + block_index;
    if (vdisk_read(global_disk, inode_block_num, buffer) != 0) return -1;

    struct inode *inodes = (struct inode *)buffer;
    struct inode *inode = &inodes[inode_index];

    if (!inode->valid) return -1;

    // Free direct blocks
    for (int i = 0; i < 4; ++i) {
        if (inode->direct[i]) {
            // zero-out the block
            memset(buffer, 0, VDISK_SECTOR_SIZE);
            vdisk_write(global_disk, inode->direct[i], buffer);
        }
    }

    // Free single-indirect block
    if (inode->indirect1) {
        vdisk_read(global_disk, inode->indirect1, buffer);
        uint32_t *pointers = (uint32_t *)buffer;
        for (int i = 0; i < 256; ++i) {
            if (pointers[i]) {
                memset(buffer, 0, VDISK_SECTOR_SIZE);
                vdisk_write(global_disk, pointers[i], buffer);
            }
        }
        memset(buffer, 0, VDISK_SECTOR_SIZE);
        vdisk_write(global_disk, inode->indirect1, buffer);
    }

    // Free double-indirect blocks
    if (inode->indirect2) {
        vdisk_read(global_disk, inode->indirect2, buffer);
        uint32_t *ind1_blocks = (uint32_t *)buffer;

        for (int i = 0; i < 256; ++i) {
            if (ind1_blocks[i]) {
                vdisk_read(global_disk, ind1_blocks[i], buffer);
                uint32_t *pointers = (uint32_t *)buffer;

                for (int j = 0; j < 256; ++j) {
                    if (pointers[j]) {
                        memset(buffer, 0, VDISK_SECTOR_SIZE);
                        vdisk_write(global_disk, pointers[j], buffer);
                    }
                }
                memset(buffer, 0, VDISK_SECTOR_SIZE);
                vdisk_write(global_disk, ind1_blocks[i], buffer);
            }
        }
        memset(buffer, 0, VDISK_SECTOR_SIZE);
        vdisk_write(global_disk, inode->indirect2, buffer);
    }

    // Invalidate inode
    inode->valid = 0;
    memset(&inode->size, 0, sizeof(struct inode) - sizeof(uint8_t));

    // Write inode block back
    memcpy(buffer, inodes, VDISK_SECTOR_SIZE);
    return vdisk_write(global_disk, inode_block_num, buffer);
}

// Convert logical block number to physical block using direct/indirect/indirect2
int resolve_block(struct inode *node, uint32_t logical_block_num) {
    uint8_t buffer[1024];

    // Direct blocks
    if (logical_block_num < 4) {
        return node->direct[logical_block_num];
    }

    // Indirect1 (single-level)
    logical_block_num -= 4;
    if (logical_block_num < 256) {
        if (node->indirect1 == 0) return 0; // Not allocated
        vdisk_read(global_disk, node->indirect1, buffer);
        uint32_t *entries = (uint32_t *)buffer;
        return entries[logical_block_num];
    }

    // Indirect2 (double-level)
    logical_block_num -= 256;
    if (logical_block_num < 256 * 256) {
        if (node->indirect2 == 0) return 0;
        uint32_t outer_index = logical_block_num / 256;
        uint32_t inner_index = logical_block_num % 256;

        // Read first-level table
        vdisk_read(global_disk, node->indirect2, buffer);
        uint32_t *indirect_blocks = (uint32_t *)buffer;
        uint32_t indirect1_block = indirect_blocks[outer_index];
        if (indirect1_block == 0) return 0;

        // Read second-level table
        vdisk_read(global_disk, indirect1_block, buffer);
        uint32_t *entries = (uint32_t *)buffer;
        return entries[inner_index];
    }

    // Beyond max size
    return 0;
}

int read(int inode_num, uint8_t *data, int len, int offset) {
    if (!global_disk) return vdisk_ENODISK;

    if (len < 0 || offset < 0) return vdisk_EINVALID;

    uint8_t block_buf[1024];
    uint8_t sb_buf[1024];
    vdisk_read(global_disk, 0, sb_buf);
    struct superblock *sb = (struct superblock *)sb_buf;

    uint32_t inode_block_start = 1;
    uint32_t inode_block_count = sb->num_inode_blocks;
    uint32_t inode_block_num = inode_num / 32;
    uint32_t inode_index = inode_num % 32;

    if (inode_block_num >= inode_block_count) return -1;

    // Read the block containing the inode
    vdisk_read(global_disk, inode_block_start + inode_block_num, block_buf);
    struct inode *node = ((struct inode *)block_buf) + inode_index;

    if (node->valid == 0) return -1;

    // Clamp read range
    if ((uint32_t)offset >= node->size) return 0;
    if ((uint32_t)offset + (uint32_t)len > node->size)
        len = node->size - (uint32_t)offset;

    int bytes_read = 0;
    while (bytes_read < len) {
        int current_offset = offset + bytes_read;
        int block_index = current_offset / 1024;
        int inner_offset = current_offset % 1024;
        int to_copy = 1024 - inner_offset;
        if (to_copy > len - bytes_read) to_copy = len - bytes_read;

        int physical_block = resolve_block(node, block_index);
        if (physical_block == 0) break; // Hole or error

        vdisk_read(global_disk, physical_block, block_buf);
        memcpy(data + bytes_read, block_buf + inner_offset, to_copy);
        bytes_read += to_copy;
    }

    return bytes_read;
}


int write(int inode_num, const uint8_t *data, int len, int offset) {
    if (!data || len < 0 || offset < 0) return vdisk_EINVALID;
    if (global_disk == NULL) {
        fprintf(stderr, "Error: Disk not mounted.\n");
        return vdisk_ENODISK;
    }

    uint8_t block[VDISK_SECTOR_SIZE];

    // Read the superblock
    if (vdisk_read(global_disk, 0, block) < 0) return -1;
    uint32_t num_blocks       = read_le32(block + 16);
    uint32_t block_size       = read_le32(block + 24);

    int inode_block = 1 + inode_num / INODES_PER_BLOCK;
    int inode_offset = (inode_num % INODES_PER_BLOCK) * 32;

    if (vdisk_read(global_disk, inode_block, block) < 0) return -1;
    uint8_t *inode = block + inode_offset;

    uint32_t valid = read_le32(inode + 0);
    uint32_t size  = read_le32(inode + 4);
    if (!valid) return vdisk_EINVALID;

    int bytes_written = 0;
    uint8_t datablock[VDISK_SECTOR_SIZE];

    int need_write_inode;
    while (bytes_written < len) {
        int file_offset  = offset + bytes_written;
        int block_index  = file_offset / block_size;
        int block_offset = file_offset % block_size;

        uint32_t block_num = 0;
        need_write_inode = 0;

        // -------- Direct blocks --------
        if (block_index < 4) {
            uint32_t *direct = (uint32_t *)(inode + 8 + block_index * 4);
            block_num = read_le32((uint8_t *)direct);
            if (block_num == 0) {
                if (block_num == 0 || block_num >= num_blocks) return bytes_written;
                write_le32((uint8_t *)direct, block_num);
                need_write_inode = 1;
            }
        }
        // -------- Single indirect block --------
        else if (block_index < 4 + 256) {
            uint32_t indir_block = read_le32(inode + 24);
            if (indir_block == 0) {
                if (indir_block == 0) return bytes_written;
                write_le32(inode + 24, indir_block);
                memset(datablock, 0, VDISK_SECTOR_SIZE);
                if (vdisk_write(global_disk, indir_block, datablock) < 0) return -1;
                need_write_inode = 1;
            }
            if (vdisk_read(global_disk, indir_block, datablock) < 0) return -1;

            uint32_t *entry = (uint32_t *)(datablock + (block_index - 4) * 4);
            block_num = read_le32((uint8_t *)entry);
            if (block_num == 0) {
                if (block_num == 0) return bytes_written;
                write_le32((uint8_t *)entry, block_num);
                if (vdisk_write(global_disk, indir_block, datablock) < 0) return -1;
            }
        }
        // -------- Double indirect block --------
        else {
            uint32_t double_indir_block = read_le32(inode + 28);
            if (double_indir_block == 0) {
                if (double_indir_block == 0) return bytes_written;
                write_le32(inode + 28, double_indir_block);
                memset(datablock, 0, VDISK_SECTOR_SIZE);
                if (vdisk_write(global_disk, double_indir_block, datablock) < 0) return -1;
                need_write_inode = 1;
            }

            if (vdisk_read(global_disk, double_indir_block, datablock) < 0) return -1;

            int remaining = block_index - 4 - 256;
            int i1 = remaining / 256;
            int i2 = remaining % 256;

            uint32_t indir1_block = read_le32(datablock + i1 * 4);
            if (indir1_block == 0) {
                if (indir1_block == 0) return bytes_written;
                write_le32(datablock + i1 * 4, indir1_block);
                memset(block, 0, VDISK_SECTOR_SIZE);
                if (vdisk_write(global_disk, indir1_block, block) < 0) return -1;
                if (vdisk_write(global_disk, double_indir_block, datablock) < 0) return -1;
            }

            if (vdisk_read(global_disk, indir1_block, datablock) < 0) return -1;

            uint32_t *entry = (uint32_t *)(datablock + i2 * 4);
            block_num = read_le32((uint8_t *)entry);
            if (block_num == 0) {
                if (block_num == 0) return bytes_written;
                write_le32((uint8_t *)entry, block_num);
                if (vdisk_write(global_disk, indir1_block, datablock) < 0) return -1;
            }
        }

        if (vdisk_read(global_disk, block_num, datablock) < 0) return -1;

        int to_write = block_size - block_offset;
        if (to_write > len - bytes_written) to_write = len - bytes_written;

        memcpy(datablock + block_offset, data + bytes_written, to_write);
        if (vdisk_write(global_disk, block_num, datablock) < 0) return -1;

        bytes_written += to_write;
    }

    if ((uint32_t) (offset + bytes_written) > size) {
        write_le32(inode + 4, offset + bytes_written);
        need_write_inode = 1;
    }

    if (need_write_inode) {
        if (vdisk_write(global_disk, inode_block, block) < 0) return -1;
    }


    return bytes_written;
}