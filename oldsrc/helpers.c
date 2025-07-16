#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/error.h"
#include "../include/vdisk.h"
#include "helpers.h"

extern const int VDISK_SECTOR_SIZE;
const int INODES_PER_BLOCK = 32;
const int SUPERBLOCK_SECTOR = 0;

int total_blocks = 0;  // Total blocks on disk
int data_start_block = 0;  // First data block

const unsigned char MAGIC_NUMBER[16] = {
    0xf0, 0x55, 0x4c, 0x49,
    0x45, 0x47, 0x45, 0x49,
    0x4e, 0x46, 0x4f, 0x30,
    0x39, 0x34, 0x30, 0x0f 
};

/*
This function is responsible for deleting a block (zeroed out).
*/
int _delete_block(DISK *disk, uint8_t *block_bitmap, uint32_t block) {
    if (block == 0) {
        return vdisk_EACCESS;
    } 

    uint8_t buffer[VDISK_SECTOR_SIZE];
    memset(buffer, 0, VDISK_SECTOR_SIZE);
    int ret = vdisk_write(disk, block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to free block %d: %d\n", block, ret);
        return ret;
    }
    mark_block_free(block_bitmap, block);
    return 0;
}

/*
This function is responsible for deleting an indirect block as well
as the blocks it points to.
*/
int _delete_single_indirect_block(DISK *disk, uint8_t *block_bitmap, uint32_t i1_block) {

    uint8_t buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(disk, i1_block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to read indirect block %d: %d\n", i1_block, ret);
        return ret;
    }

    uint32_t *pointers = (uint32_t *)buffer;

    int num_pointers = 256;

    // Free each data block pointed to by the indirect block
    for (int i = 0; i < num_pointers; i++) {
        if (pointers[i] != 0) {
            ret = _delete_block(disk, block_bitmap, pointers[i]);
            if (ret != 0) return ret;
        }
    }

    // Free the indirect block itself
    return _delete_block(disk, block_bitmap, i1_block);
}

/*
This function is responsible for deleting a double indirect block as
well as the indirect block and the blocks it points to.
*/
int _delete_double_indirect_block(DISK *disk, uint8_t *block_bitmap, uint32_t i2_block) {

    uint8_t buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(disk, i2_block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to read double indirect block %d: %d\n", i2_block, ret);
        return ret;
    }

    uint32_t *pointers = (uint32_t *)buffer;
    int num_pointers = 256;

    // Free each indirect block pointed by the double indirect block
    for (int i = 0; i < num_pointers; i++) {
        if (pointers[i] != 0) {
            ret = _delete_single_indirect_block(disk, block_bitmap, pointers[i]);
            if (ret != 0) return ret;
        }
    }

    // Free the double indirect block itself
    return _delete_block(disk, block_bitmap, i2_block);
    return 0;
}


/*
This function retrieves all the addresses of data blocks from an inode
based on the specified range [first_block, last_block].
*/
uint32_t *_get_block_addresses(DISK* disk, struct inode *inode, int first_block, int last_block) {
    if (first_block > last_block || first_block < 0) {
        fprintf(stderr, "Invalid block range.\n");
        return NULL;
    }

    int total_blocks = last_block - first_block + 1;
    uint32_t *block_addresses = malloc(total_blocks * sizeof(uint32_t));
    if (block_addresses == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    int count = 0;

    // Process direct blocks
    for (int i = 0; i < 4 && count < total_blocks; i++) {
        if (first_block <= i && i <= last_block) {
            block_addresses[count++] = inode->direct[i];
        }
    }

    // Process single indirect block
    if (inode->indirect1 != 0 && count < total_blocks) {
        uint8_t buffer[VDISK_SECTOR_SIZE];
        int ret = vdisk_read(disk, inode->indirect1, buffer);
        if (ret != 0) {
            fprintf(stderr, "Failed to read single indirect block: %d\n", ret);
            free(block_addresses);
            return NULL;
        }

        uint32_t *indirect_blocks = (uint32_t *)buffer;
        for (int i = 0; i < 256 && count < total_blocks; i++) {
            int block_index = 4 + i;
            if (first_block <= block_index && block_index <= last_block) {
                block_addresses[count++] = indirect_blocks[i];
            }
        }
    }

    // Process double indirect block
    if (inode->indirect2 != 0 && count < total_blocks) {
        uint8_t buffer[VDISK_SECTOR_SIZE];
        int ret = vdisk_read(disk, inode->indirect2, buffer);
        if (ret != 0) {
            fprintf(stderr, "Failed to read double indirect block: %d\n", ret);
            free(block_addresses);
            return NULL;
        }

        uint32_t *double_indirect_blocks = (uint32_t *)buffer;
        for (int i = 0; i < 256 && count < total_blocks; i++) {
            if (double_indirect_blocks[i] != 0) {
                uint8_t indirect_buffer[VDISK_SECTOR_SIZE];
                ret = vdisk_read(disk, double_indirect_blocks[i], indirect_buffer);
                if (ret != 0) {
                    fprintf(stderr, "Failed to read indirect block %d: %d\n", double_indirect_blocks[i], ret);
                    free(block_addresses);
                    return NULL;
                }

                uint32_t *indirect_blocks = (uint32_t *)indirect_buffer;
                for (int j = 0; j < 256 && count < total_blocks; j++) {
                    int block_index = 4 + 256 + i * 256 + j;
                    if (first_block <= block_index && block_index <= last_block) {
                        block_addresses[count++] = indirect_blocks[j];
                    }
                }
            }
        }
    }

    return block_addresses;
}


// Check if a block is used (1 = used, 0 = free)
int is_block_used(uint8_t *block_bitmap, int block_num) {
    if (block_num < 0 || block_num >= total_blocks) return 1;  // Out of bounds = treat as used
    return block_bitmap[block_num] == 1;
}

// Mark a block as used
void mark_block_used(uint8_t *block_bitmap, int block_num) {
    if (block_num < 0 || block_num >= total_blocks) return;    
    block_bitmap[block_num] = 1;
}

// Mark a block as free
void mark_block_free(uint8_t *block_bitmap, int block_num) {
    if (block_num < 0 || block_num >= total_blocks) return;
    block_bitmap[block_num] = 0;
}

// Find the first free block (returns -1 if none found)
int find_first_free_block(uint8_t *block_bitmap) {
    for (int i = data_start_block; i < total_blocks; i++) {
        if (!is_block_used(block_bitmap, i)) {
            mark_block_used(block_bitmap, i);  // Mark it as used
            return i;
        }
    }
    return -1;  // No free blocks
}

// Reads a 32-bit little-endian value from a byte array
uint32_t read_le32(const uint8_t *p) 
{
    // Assemble the 32-bit integer by combining 4 bytes in little-endian order
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// Write a 32-bit integer in little-endian format
void write_le32(uint8_t *p, uint32_t val) {
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}
