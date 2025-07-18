#ifndef SSFS_INTERNAL_H
#define SSFS_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "vdisk.h"

// #########################
// # Structure definitions #
// #########################

struct superblock {
    uint8_t magic[16];
    uint32_t num_blocks;
    uint32_t num_inode_blocks;
    uint32_t block_size;
} __attribute__((packed));

typedef struct superblock superblock_t;

struct inode {
    uint32_t valid;      // 0 for unused, 1 for used
    uint32_t size;       // File size in bytes
    uint32_t direct[4];  // Direct block "pointers"
    uint32_t indirect1;  // First indirect "pointer"
    uint32_t indirect2;  // Second indirect "pointer"
} __attribute__((packed));

typedef struct inode inode_t;

typedef inode_t inodes_block_t[32];

// ####################
// # Global variables #
// ####################

extern DISK *disk_handle;
extern bool *allocated_blocks_handle;

// #############
// # Constants #
// #############

extern const int VDISK_SECTOR_SIZE;  // from vdisk.c, defaults to 1024
extern const int SUPERBLOCK_SECTOR;
extern const unsigned char MAGIC_NUMBER[];

// ##########################
// # Prototypes declaration #
// ##########################

int is_mounted();
int is_inode_positive(int inodes_num);
int is_inode_valid(int inodes_num, int max_inodes_num);
int is_magic_ok(uint8_t * number);
int _initialize_allocated_blocks();
int allocate_block(uint32_t block);
int _allocate_indirect_block(uint32_t indirect_block);
int _allocate_double_indirect_block(uint32_t double_indirect_block);


#endif