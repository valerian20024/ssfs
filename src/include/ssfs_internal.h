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

// ####################
// # Global variables #
// ####################

extern DISK *global_disk_handle;
extern bool *allocated_blocks;

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
int allocate_block(bool* bitmap, uint32_t block);
int _initialize_allocated_blocks();

#endif