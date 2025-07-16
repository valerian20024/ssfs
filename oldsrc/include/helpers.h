#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>
#include "vdisk.h"

extern const int VDISK_SECTOR_SIZE;
extern const int INODES_PER_BLOCK;
extern const int SUPERBLOCK_SECTOR;
extern int total_blocks;
extern int data_start_block;

// Structure definitions
struct superblock {
    uint8_t magic[16];
    uint32_t num_blocks;
    uint32_t num_inode_blocks;
    uint32_t block_size;
} __attribute__((packed));

struct inode {
    uint32_t valid;      // 0 for unused, 1 for used
    uint32_t size;       // File size in bytes
    uint32_t direct[4];  // Direct block "pointers"
    uint32_t indirect1;  // First indirect "pointer"
    uint32_t indirect2;  // Second indirect "pointer"
} __attribute__((packed));

struct inode_block {
    struct inode inodes[32];
};

int _delete_block(DISK *disk, uint8_t *block_bitmap, uint32_t block);
int _delete_single_indirect_block(DISK *disk, uint8_t *block_bitmap, uint32_t i1_block);
int _delete_double_indirect_block(DISK *disk, uint8_t *block_bitmap, uint32_t i2_block);
uint32_t *_get_block_addresses(DISK* disk, struct inode *inode, int first_block, int last_block);
int is_block_used(uint8_t *block_bitmap, int block_num);
void mark_block_used(uint8_t *block_bitmap, int block_num);
void mark_block_free(uint8_t *block_bitmap, int block_num);
int find_first_free_block();
uint32_t read_le32(const uint8_t *p); 
void write_le32(uint8_t *p, uint32_t val);


#endif
