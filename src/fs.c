#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/error.h"
#include "../include/vdisk.h"


// Constants
extern int VDISK_SECTOR_SIZE;
const unsigned char MAGIC_NUMBER[16] = {
    0xf0, 0x55, 0x4c, 0x49,
    0x45, 0x47, 0x45, 0x49,
    0x4e, 0x46, 0x4f, 0x30,
    0x39, 0x34, 0x30, 0x0f 
};

const int SUPERBLOCK_SECTOR = 0;

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

// Global disk handle
static DISK * global_disk = NULL;

/*
This function formats, that is, installs SSFS on, the virtual
disk whose disk image is contained in file disk_name (as a c-style string). It will attempt
to construct an SSFS instance with at least inodes i-nodes and a minimum of a single data
block. inodes defaults to 1 if this argument is 0 or negative.
Note that this function must refuse to format a mounted disk.
*/
int format(char *disk_name, int inodes) {
    // check inodes value : defaults to 1 if 0 or negative
    if (inodes <= 0) { inodes = 1; }    

    if (global_disk != NULL) {
        fprintf(stderr, "Disk is already mounted. Unmount it before formatting.\n");
        return vdisk_EACCESS;
    }
    
    // Initialize the disk
    DISK disk;
    int ret = vdisk_on(disk_name, &disk);
    if (ret != 0) return ret;

    // Verify there is at enough blocks for inodes and 1 data block and 1 superblock
    // There are 32 inodes (32 bytes) per block (1024 bytes)
    uint32_t inode_blocks = (inodes + 31) / 32;
    if (disk.size_in_sectors < inode_blocks + 2) {
        vdisk_off(&disk);
        return vdisk_ENOSPACE;
    }

    // Initialize superblock
    struct superblock sb;
    memcpy(sb.magic, MAGIC_NUMBER, 16);
    sb.num_blocks = disk.size_in_sectors - 1; // Reserve the first sector for the superblock
    sb.num_inode_blocks = inode_blocks;
    sb.block_size = VDISK_SECTOR_SIZE;

    // Declaring buffer the size of one block
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // fulling it with zeros
    memset(buffer, 0, VDISK_SECTOR_SIZE);

    // Zeroing the whole volume (with superblock)
    for (uint32_t i = 0; i < disk.size_in_sectors; i++) {
        ret = vdisk_write(&disk, i, buffer);
        if (ret != 0) {
            vdisk_off(&disk);
            return ret;
        }
    }

    // Then rewriting with the superblock data in the beginning
    memcpy(buffer, &sb, sizeof(struct superblock));
    ret = vdisk_write(&disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) {
        vdisk_off(&disk);
        return ret;
    }

    // Sync changes to disk
    ret = vdisk_sync(&disk);
    if (ret != 0) {
        vdisk_off(&disk);
        return ret;
    }

    // Close the disk
    vdisk_off(&disk);
    fprintf(stdout, "Disk formatted successfully.\n");
    return 0;
}

int stat(int inode_num) {
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;  // Error: disk not mounted
    }

    // Calculate the block and offset for the inode
    const int inodes_per_block = 32;
    int block = inode_num / inodes_per_block;  // Which inode block
    int offset = inode_num % inodes_per_block;  // Offset within the block

    // Read the inode block
    uint8_t buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(global_disk, 1 + block, buffer);  // Inode blocks start at sector 1
    if (ret != 0) {
        fprintf(stderr, "Failed to read inode block %d: %d\n", 1 + block, ret);
        return -1;
    }

    struct inode *inodes = (struct inode *)buffer;
    struct inode *target_inode = &inodes[offset];

    // Check if the inode is in use
    if (target_inode->valid == 0) {
        fprintf(stderr, "Inode %d is not in use.\n", inode_num);
        return -1;
    }

    // Return the file size
    return (int)target_inode->size;  // Cast uint32_t to int for return value
}

int mount(char *disk_name) {
    // Cannot mount if already mounted
    if (global_disk != NULL) {
        fprintf(stderr, "Disk is already mounted. Unmount it before mounting a new one.\n");
        return vdisk_EACCESS;
    }

    // Initialize the disk
    DISK disk;
    int ret = vdisk_on(disk_name, &disk);
    if (ret != 0) {
        return ret;
    }
    
    // Check validity of SSFS partition
    uint8_t buffer[VDISK_SECTOR_SIZE];
    ret = vdisk_read(&disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) {
        vdisk_off(&disk);
        return ret;
    }
    struct superblock *sb = (struct superblock *)buffer;
    if (memcmp(sb->magic, MAGIC_NUMBER, 16) != 0) {
        vdisk_off(&disk);
        return vdisk_EINVALID;
    }

    // Disk is valid, set global state
    global_disk = malloc(sizeof(DISK));
    if (global_disk == NULL) {
        vdisk_off(&disk);
        return -1;
    }
    *global_disk = disk;

    // Update the global disk pointer
    return 0;
}

int unmount() {
    // Fails if no disk is mounted
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return vdisk_ENODISK;
    }

    // Update the global disk pointer to NULL
    free(global_disk);    
    global_disk = NULL;
    return 0;
}

int create() {
    // Check if disk is mounted
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return vdisk_ENODISK;
    }

    uint8_t buffer[VDISK_SECTOR_SIZE];

    int ret = vdisk_read(global_disk, SUPERBLOCK_SECTOR, buffer);
    if (ret != 0) {
        return ret;
    }

    struct superblock *sb = (struct superblock *)buffer;
    uint32_t inode_blocks = sb->num_inode_blocks;

    // The inode number to be returned, acts as an index
    uint32_t inode_num = 0;

    for (uint32_t i = 0; i < inode_blocks; i++) {
        ret = vdisk_read(global_disk, SUPERBLOCK_SECTOR + 1 + i, buffer);
        if (ret != 0) {
            return ret;
        }
        // Casting to a block of inodes
        struct inode_block *ib = (struct inode_block *)buffer;

        // Looking through the inodes
        for (int j = 0; j < 32; j++) {
            // An available inode is found
            if (ib->inodes[j].valid == 0) {
                inode_num = i * 32 + j;
                ib->inodes[j].valid = 3;
                
                // Write back the modified inode block
                ret = vdisk_write(global_disk, SUPERBLOCK_SECTOR + 1 + i, buffer);
                if (ret != 0) {
                    return ret;
                }
                int ret = vdisk_sync(global_disk);
                if (ret != 0) {
                    return ret;
                }
                
                fprintf(stdout, "Inode %d created successfully.\n", inode_num);
                return inode_num;
            }
        }
    }

    // Haven't found any available inode
    fprintf(stderr, "No available inode found.\n");
    return vdisk_ENOSPACE;
}

/*
This function is responsible for deleting block (zeroed out).
*/
// todo : use this function in the other parts of the code.
int _delete_block(uint32_t block) {
    if (block == 0) {
        return vdisk_EACCESS;
    } 

    uint8_t buffer[VDISK_SECTOR_SIZE];
    memset(buffer, 0, VDISK_SECTOR_SIZE);
    int ret = vdisk_write(global_disk, block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to free block %d: %d\n", block, ret);
        return ret;
    }
    return 0;
}

/*
This function is responsible for deleting an indirect block as well
as the blocks it points to.
*/
int _delete_single_indirect_block(uint32_t i1_block) {

    uint8_t buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(global_disk, i1_block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to read indirect block %d: %d\n", i1_block, ret);
        return ret;
    }

    uint32_t *pointers = (uint32_t *)buffer;

    int num_pointers = 256;

    // Free each data block pointed to by the indirect block
    for (int i = 0; i < num_pointers; i++) {
        if (pointers[i] != 0) {
            ret = _delete_block(pointers[i]);
            if (ret != 0) return ret;
        }
    }

    // Free the indirect block itself
    return _delete_block(i1_block);
}

/*
This function is responsible for deleting a double indirect block as
well as the indirect block and the blocks it points to.
*/
int _delete_double_indirect_block(uint32_t i2_block) {

    uint8_t buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(global_disk, i2_block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to read double indirect block %d: %d\n", i2_block, ret);
        return ret;
    }

    uint32_t *pointers = (uint32_t *)buffer;
    int num_pointers = 256;

    // Free each indirect block pointed by the double indirect block
    for (int i = 0; i < num_pointers; i++) {
        if (pointers[i] != 0) {
            ret = _delete_single_indirect_block(pointers[i]);
            if (ret != 0) return ret;
        }
    }

    // Free the double indirect block itself
    return _delete_block(i2_block);
    return 0;
}


/*
This function is responsible for deleting a file, hence resetting the inode
and freeing the blocks it points to.
*/
int delete(int inode_num) {
    // Check if the disk is mounted
    if (global_disk == NULL) {
        fprintf(stderr, "No disk is mounted.\n");
        return -1;  // Error: disk not mounted
    }

    // Calculate the block and offset for the inode
    const int inodes_per_block = 32;
    int block = inode_num / inodes_per_block;  // Which inode block
    int offset = inode_num % inodes_per_block;  // Offset within the block

    // Read the inode block
    uint8_t buffer[VDISK_SECTOR_SIZE];
    int ret = vdisk_read(global_disk, 1 + block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to read inode block %d: %d\n", 1 + block, ret);
        return ret;
    }

    struct inode *inodes = (struct inode *)buffer;
    struct inode *target_inode = &inodes[offset];

    // Check if the inode is in use
    if (target_inode->valid == 0) {
        fprintf(stderr, "Inode %d is not in use.\n", inode_num);
        return -1;
    }

    // Free direct blocks and inode direct fields
    for (int i = 0; i < 4; i++) {
        if (target_inode->direct[i] != 0) {
            ret = _delete_block(target_inode->direct[i]);
            if (ret != 0) return ret;
            target_inode->direct[i] = 0;
        }
    }

    // Free Indirect block and inode i1 field
    if (target_inode->indirect1 != 0) {
        ret = _delete_single_indirect_block(target_inode->indirect1);
        if (ret != 0) return ret;
        target_inode->indirect1 = 0;
    }

    // Free Double indirect block and inode i2 field
    if (target_inode->indirect2 != 0) {
        ret = _delete_double_indirect_block(target_inode->indirect2);
        if (ret != 0) return ret;
        target_inode->indirect2 = 0;
    }

    // Mark the inode as unused and clear its fields
    target_inode->valid = 0;
    target_inode->size = 0;

    // Write the updated inode block back to disk
    ret = vdisk_write(global_disk, 1 + block, buffer);
    if (ret != 0) {
        fprintf(stderr, "Failed to write inode block %d: %d\n", 1 + block, ret);
        return ret;
    }

    // Sync changes to disk
    ret = vdisk_sync(global_disk);
    if (ret != 0) {
        fprintf(stderr, "Failed to sync disk: %d\n", ret);
        return ret;
    }

    fprintf(stdout, "Inode %d deleted successfully.\n", inode_num);
    return 0;
}

int read(int inode_num, uint8_t *data, int len, int offset) {
    inode_num++;
    data++;
    len++;
    offset++;
    return 0;
}

int write(int inode_num, uint8_t *data, int len, int offset) {
    inode_num++;
    data++;
    len++;
    offset++;
    return 0;
}

