//! this file aims at defining helpers functions used in several
//! places in the code.

// todo write file documentation
// todo write lines not longer than 80 chars

// The functions in this file often return 0 for false condition and 1 for true conditions. This allows to call them in the following way :
// if (is_magic_ok()) { ... } which will effectively execute if
// the magic number is good.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "vdisk.h"
#include "ssfs_internal.h"
#include "error.h"

const uint8_t MAGIC_NUMBER[16] = {
    0xf0, 0x55, 0x4c, 0x49,
    0x45, 0x47, 0x45, 0x49,
    0x4e, 0x46, 0x4f, 0x30,
    0x39, 0x34, 0x30, 0x0f 
};

// This function returns 0 is the disk is not mounted and 1 otherwise.
int is_mounted() {
    return disk_handle == NULL ? 0 : 1;
}

// This function returns 0 is the inode is negative, 1 if it's positive or zero.
int is_inode_positive(int inode_num) {
    return inode_num >= 0;
}

// This function returns 0 is the inode is not valid and 1 if it is. 
// Valid means to be strictly positive and not overflow the maximum number of inodes in the filesystem.
int is_inode_valid(int inode_num, int max_inode_num) {
    return is_inode_positive(inode_num) && inode_num <= max_inode_num;
}

// This function returns 0 if the magic numbers don't correspond. Return 1 if they correspond.
int is_magic_ok(uint8_t *number) {
    int ret = memcmp(number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
    return ret == 0 ? 1 : 0;
}


int erase_block_content(uint32_t block_num) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];
    memset(buffer, 0, VDISK_SECTOR_SIZE);

    ret = vdisk_write(disk_handle, block_num, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup;
    }

    ret = vdisk_sync(disk_handle);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup;
    }

cleanup:
    return ret;
}

/**
 * @brief Sets the allocation status of a specific block in the file system's bitmap.
 *
 * This function updates the internal block allocation bitmap, marking a given
 * block as either in use or free.
 *
 * @param block The numerical identifier (block number) of the block whose status is to be changed.
 * @param status A boolean value indicating the new status: `true` for allocated, `false` to mark it as free.
 *
 * @return 0 on success.
 * @return Negative integers (erros codes) on failue.
 *
 * @note It is typically used by higher-level routines such as `allocate_block` but can
 * also be used directly.
 */
int set_block_status(uint32_t block, bool status) {
    if (allocated_blocks_handle == NULL) 
        return ssfs_EALLOC;

    if (status == false) 
        erase_block_content(block);

    allocated_blocks_handle[block] = status;
    return 0;
}

/**
 * @brief Sets the allocation status to "in use" of a specific block in the bitmap.
 *
 * @param block The numerical identifier (block number) of the block whose status is to be changed.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 * 
 */
int allocate_block(uint32_t block) {
    return set_block_status(block, true);
}

/**
 * @brief Sets the allocation status to "free" of a specific block in the bitmap.
 *
 * @param block The numerical identifier (block number) of the block whose status is to be changed.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 * 
 */
int deallocate_block(uint32_t block) {
    return set_block_status(block, false);
}


/**
 * @brief Sets an indirect block and all associated data blocks status.
 *
 * This function is updates the internal block allocation bitmap, marking a given 
 * indirect blocks as well as the data blocks it points to, as either in use or free.
 * @param indirect_block The logical block number of the indirect block whose
 * status should change.
 * @param status A boolean value indicating the new status: `true` for allocated, 
 * `false` to mark it as free.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 *
 * @note It is typically used by higher-level routines such as 'allocate_indirect_block`.
 */
int _update_indirect_block_status(uint32_t indirect_block, bool status) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    ret = vdisk_read(disk_handle, indirect_block, buffer);
    if (ret != 0)
        goto cleanup;

    uint32_t *data_blocks = (uint32_t *)buffer;
    for (int db = 0; db < 256; db++) {
        if (data_blocks[db])
            set_block_status(data_blocks[db], status);
    }
    set_block_status(indirect_block, status);

cleanup:
    return ret;
}

/**
 * @brief Sets the allocation status to "in use" for an indirect block and the blocks it
 * points to in the bitmap.
 *
 * @param block The numerical identifier (block number) of the indirect block that 
 * should be freed.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 * 
 */
int allocate_indirect_block(uint32_t indirect_block) {
    return _update_indirect_block_status(indirect_block, true);
}

/**
 * @brief Sets the allocation status to "free" for an indirect block and the blocks it
 * points to in the bitmap.
 *
 * @param block The numerical identifier (block number) of the indirect block that 
 * should be freed.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 * 
 */
int deallocate_indirect_block(uint32_t indirect_block) {
    return _update_indirect_block_status(indirect_block, false);
}

/**
 * @brief Sets a double indirect block and all associated indirect and data blocks status.
 *
 * This function is updates the internal block allocation bitmap, marking a given
 * double indirect block, the indirect blocks it points to as well as the data blocks
 * they point to, as either in use or free.
 * @param double_indirect_block The logical block number of the double indirect block
 * whose status should change.
 * @param status A boolean value indicating the new status: `true` for allocated, `false` to mark it as free.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 *
 * @note It is typically used by higher-level routines such as
 * `allocate_double_indirect_block`.
 */
int _update_double_indirect_block_status(uint32_t double_indirect_block, bool status) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];  // storing 2-indirect block

    ret = vdisk_read(disk_handle, double_indirect_block, buffer);
    if (ret != 0)
        goto cleanup;

    uint32_t *indirect_ptrs = (uint32_t *)buffer;
    for (int ip = 0; ip < 256; ip++) {
        if (indirect_ptrs[ip] == 0)  // todo change this
            continue;

        _update_indirect_block_status(indirect_ptrs[ip], status);
    }

    set_block_status(double_indirect_block, status);

cleanup:
    return ret;
}

/**
 * @brief Sets the allocation status to "in use" of a double indirect block and all associated indirect and data blocks.
 *
 * @param double_indirect_block The numerical identifier (block number) of the block whose status is to be changed.
 * @param status A boolean value indicating the new status: `true` for allocated, `false` to mark it as free.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 * 
 */
int allocate_double_indirect_block(uint32_t double_indirect_block) {
    return _update_double_indirect_block_status(double_indirect_block, true);
}

/**
 * @brief Sets the allocation status to "free" of a double indirect block and all associated indirect and data blocks.
 *
 * @param double_indirect_block The numerical identifier (block number) of the block whose status is to be changed.
* @param status A boolean value indicating the new status: `true` for allocated, `false` to mark it as free.
 *
 * @return 0 on success.
 * @return Negative integers (error codes) on failure.
 * 
 */
int deallocate_double_indirect_block(uint32_t double_indirect_block) {
    return _update_double_indirect_block_status(double_indirect_block, false);
}