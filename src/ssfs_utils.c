/*
 * ssfs_utils.c
 * ============
 * 
 * This file defines all the shared functions used in several places in the
 * program.
 * It also defines the MAGIC_NUMBER constant, that allows to identify the
 * filesystem type.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "vdisk.h"
#include "ssfs_internal.h"
#include "error.h"

// #############
// # Constants #
// #############

const char *COLOR_RESET     = "\033[0m";
const char *COLOR_RED       = "\033[1;31m";
const char *COLOR_GREEN     = "\033[1;32m";
const char *COLOR_YELLOW    = "\033[1;33m";
const char *COLOR_BLUE      = "\033[1;34m";
const char *COLOR_MAGENTA   = "\033[1;35m";
const char *COLOR_CYAN      = "\033[1;36m";
const char *COLOR_WHITE     = "\033[1;37m";

// The filesystem's magic number. 
const uint8_t MAGIC_NUMBER[16] = {
    0xf0, 0x55, 0x4c, 0x49,
    0x45, 0x47, 0x45, 0x49,
    0x4e, 0x46, 0x4f, 0x30,
    0x39, 0x34, 0x30, 0x0f 
};

/**
 * @brief Checks if the disk has already been mounted.
 * @return 0 if not mounted
 * @return 1 if mounted
 * 
 */
int is_mounted() {
    return disk_handle == NULL ? 0 : 1;
}

/**
 * @brief This function checks if the inode number is positive.
 * @return 0 if the inode is strictly negative.
 * @return 1 if the inode is positive (or zero).
 */
int is_inode_positive(int inode_num) {
    return inode_num >= 0;
}

/**
 * @brief This function operates common checks on the given inode value.
 * 
 * It will check if it's positive and if it doesn't goes out of bound of
 * the filesystem.
 * 
 * @return 0 if the inode is not valid.
 * @return 1 if the inode is valid.
 * 
 */
int is_inode_valid(int inode_num, int max_inode_num) {
    return is_inode_positive(inode_num) && inode_num <= max_inode_num;
}

/**
 * @brief Compares a given number to the magic number of the filesystem.
 * @return 0 if the magic numbers don't correspond. 
 * @return 1 if they correspond.
 * 
 */
int is_magic_ok(uint8_t *number) {
    int ret = memcmp(number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
    return ret == 0 ? 1 : 0;
}

//! wtf is this ????
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




/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <blue>label:<white>formatted_message<reset>\n
 * If format is NULL or empty, prints just the label in blue.
 *
 * @param label The label to print (e.g., "Mounting").
 * @param format The format string for the message (can be NULL).
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_info("Mounting", "%s", disk_name); where disk_name is a string.
 * @note Example: print_info("A simple label...", NULL);
 * 
 */
void print_info(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_BLUE, label, format, args);
    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <green>label:formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_error("Error", "%d", error_code); where error_code is an integer.
 * 
 */
void print_error(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_RED, label, format, args);
    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <green>label:formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_success("Success", "%d", return_code); where return_code is an integer.
 * 
 */
void print_success(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_GREEN, label, format, args);
    va_end(args);
}


/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <yellow>label:formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Example: print_warning("Warning", "%d", return_code); where return_code is an integer.
 * 
 */
void print_warning(const char *label, const char *format, ...) {
    va_list args;
    va_start(args, format);
    pretty_print(COLOR_YELLOW, label, format, args);
    va_end(args);
}

/**
 * @brief Prints a formatted message with colored label and content.
 *
 * This function prints a message to stdout in the format:
 * <color>label:<white>formatted_message<reset>\n
 *
 * @param label The label to print.
 * @param format The format string for the message.
 * @param ... Variable arguments for the format string.
 * 
 * @note Typically called by higher functions such as print_info.
 * 
 */
void pretty_print(const char* color, const char *label, const char *format, va_list args) {
    if (format == NULL || format[0] == '\0') {
        fprintf(stdout, "%s%s%s\n", color, label, COLOR_RESET);
    } else {
        fprintf(stdout, "%s%s:%s ", color, label, COLOR_WHITE);
        vfprintf(stdout, format, args);
        fprintf(stdout, "%s\n", COLOR_RESET);
    }
}


/**
 * @brief Prints detailed information about a file given its inode number.
 *
 * Prints the inode's metadata (valid, size, direct blocks, indirect pointers)
 * and lists all non-zero physical block numbers referenced by indirect1 and
 * indirect2 blocks.
 * @note This function is for developement purpose, it assumes the given input 
 * are correct, hence it will not perform many tests on them.
 * @param inode_num The inode number of the file.
 * @return 0 on success, negative error code on failure.
 */
int print_inode_info(int inode_num) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Check if filesystem is mounted
    if (!is_mounted()) {
        print_error("Filesystem not mounted", NULL);
        return ssfs_EMOUNT;
    }

    // Read inode block
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num = inode_num % 32;
    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        print_error("Failed to read inode block", "%d", ret);
        return vdisk_EACCESS;
    }
    inodes_block_t *ib = (inodes_block_t *)buffer;
    inode_t *inode = &ib[0][target_inode_num];

    // Print basic inode information
    print_info("Reading inode %d", "%d", inode_num);
    printf("  inode->valid: %d\n", inode->valid);
    printf("  inode->size: %d\n", inode->size);
    printf("  inode->direct[0]: %u\n", inode->direct[0]);
    printf("  inode->direct[1]: %u\n", inode->direct[1]);
    printf("  inode->direct[2]: %u\n", inode->direct[2]);
    printf("  inode->direct[3]: %u\n", inode->direct[3]);
    
    printf("  inode->indirect1: %u\n", inode->indirect1);
    if (inode->indirect1 != 0) {
        uint8_t indirect_buffer[VDISK_SECTOR_SIZE];
        ret = vdisk_read(disk_handle, inode->indirect1, indirect_buffer);
        if (ret != 0)
            return ret;

        uint32_t *data_block = (uint32_t *)indirect_buffer;
        for (int i = 0; i < 256; i++) {
            if (data_block[i] != 0)
                printf("    indirect1[%d] = %u\n", i, data_block[i]);
        }
    }

    printf("  inode->indirect2: %u\n", inode->indirect2);
    if (inode->indirect2 != 0) {
        uint8_t indirect2_buffer[VDISK_SECTOR_SIZE];
        ret = vdisk_read(disk_handle, inode->indirect1, indirect2_buffer);
        if (ret != 0)
            return ret;

        uint32_t *inode_block = (uint32_t *)indirect2_buffer;
        for (int i = 0; i < 256; i++) {
            if (inode_block[i] != 0) {
                printf("    indirect2[%d] = %u\n", i, inode_block[i]);
                
                uint8_t indirect_buffer[VDISK_SECTOR_SIZE];
                ret = vdisk_read(disk_handle, inode->indirect1, indirect_buffer);
                if (ret != 0)
                    return ret;

                uint32_t *data_block = (uint32_t *)indirect_buffer;
                for (int j = 0; j < 256; j++) {
                    if (data_block[j] != 0)
                        printf("      indirect2[%d][%d] = %u\n", i, j, data_block[i]);
                }
            }
        }
    }
    

    return 0;
}

/*
int print_inode_info(inode *inode) {

}




*/