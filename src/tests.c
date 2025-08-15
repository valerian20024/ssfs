/*
 * tests.c
 * =======
 * 
 * This file only serves testing purposes.
 * It defines functions for different tests.
 *
 */


#include "ssfs_internal.h"
#include "fs.h"
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

const char *COLOR_RESET     = "\033[0m";
const char *COLOR_RED       = "\033[1;31m";
const char *COLOR_GREEN     = "\033[1;32m";
const char *COLOR_YELLOW    = "\033[1;33m";
const char *COLOR_BLUE      = "\033[1;34m";
const char *COLOR_MAGENTA   = "\033[1;35m";
const char *COLOR_CYAN      = "\033[1;36m";
const char *COLOR_WHITE     = "\033[1;37m";

// format, mount, create, stats, delete, create, unmount
void test1() {
    print_warning("Starting test1...", NULL);

    char *disk_name = "testdisk.img";
    int inodes = 200;

    print_info("Formatting", "%s", disk_name);
    print_info("Number of inodes", "%d", inodes);
    format(disk_name, inodes);

    print_info("Mounting...", NULL);
    mount(disk_name);
    
    srand((unsigned int)time(NULL));
    int files_num = (rand() % (inodes / 2 - 1)) + 1; // at least 1, at most inodes / 2 - 1
    int delete_files_num = (rand() % files_num) + 1; // at least 1, at most files_num

    print_info("Number of file to be created", "%d", files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        if (file >= 0)
            printf("Created file with inode %d\n", file);
        else
            print_error("Error when creating file: ", "%d", file);
    }

    print_info("Number of statistics", "%d", files_num);
    for (int f = 0; f < files_num; f++) {
        int ret = stat(f);
        if (ret >= 0)
            printf("Size(%d) -> %d bytes\n", f, ret);
        else
            print_error("Statistics error", "%d", ret);
    }
    
    print_info("Number of files to be deleted", "%d", delete_files_num);
    for (int f = 0; f < delete_files_num; f++) {
        int ret = delete(f);
        if (ret == 0)
            printf("Deleted file number: %d\n", f);
        else
            print_error("Error when deleting file number", "%d", f);
    }

    files_num = (rand() % (inodes / 2 - 1)) + 1;

    print_info("Number of file to be created", "%d", files_num);
    for (int f = 0; f < files_num; f++) {
        int file = create();
        if (file >= 0)
            printf("Created file with inode %d\n", file);
        else
            print_error("Error when creating file: ", "%d", file);
    }

    print_info("Unmounting...", NULL);
    unmount();
}

// Reading some files with different lengths and offsets
void test2() {
    print_warning("Starting test2...", NULL);

    int bytes_num = 14558;
    print_info("Allocating ressources", "%d", bytes_num);
    uint8_t *data = malloc(bytes_num);

    int inodes[]    = {4};
    int lens[]      = {bytes_num};
    int offsets[]   = {0};

    int num_inodes  = sizeof(inodes)    / sizeof(inodes[0]);
    int num_lens    = sizeof(lens)      / sizeof(lens[0]);
    int num_offsets = sizeof(offsets)   / sizeof(offsets[0]);

    char *disk_name = "disk_img.3.bin";
    print_info("Mounting", "%s", disk_name);
    mount(disk_name);

    for (int i = 0; i < num_inodes; i++) {
        for (int l = 0; l < num_lens; l++) {
            for (int o = 0; o < num_offsets; o++) {
                int inode = inodes[i];
                int len = lens[l];
                int offset = offsets[o];

                // Creating output file for hex dump of the content
                char file_name[256];
                snprintf(
                    file_name, 
                    sizeof(file_name),
                    "output/output_inode_%d_len_%d_offset_%d.hex",
                    inode,
                    len, 
                    offset
                );

                FILE *hex_output = fopen(file_name, "w");
                if (hex_output == NULL) {
                    print_error("Failed to open output file", "%s", file_name);
                    free(data);
                    return;
                }

                print_info("Reading parameters", NULL);
                print_info("inode: ",   "%d", inode);
                print_info("len: ",     "%d", len);
                print_info("offset: ",  "%d", offset);

                print_info("Statistics... ", NULL);
                int size = stat(inode);
                if (size >= 0)
                    printf("size(%d) = %d\n", inode, size);
                else
                    print_error("Error when reading", "%d", size);

                int bytes = read(inode, data, len, offset);
                if (bytes >= 0)
                    print_success("Number of bytes successfully read", "%d", bytes);
                else
                    print_error("Error when reading", "%d", bytes);
                
                print_info("Writing data to", "%s", file_name);
                for (int i = 0; i < bytes; i++) {
                    fprintf(stdout, "%02x", data[i]);
                    fprintf(hex_output, "%02x", data[i]);
                }
                fprintf(stdout, "\n");

                fclose(hex_output);
            }
        }
    }

    print_info("Unmounting...", NULL);
    free(data);
    unmount();
}

void test3() {
    print_warning("Starting test3...", NULL);

    int bytes_num = 1000;
    print_info("Allocating resources", "%d", bytes_num);
    uint8_t *data = malloc(bytes_num);
    if (!data) {
        print_error("Memory allocation failed", NULL);
        return;
    }

    // Fill data with a pattern
    for (int i = 0; i < bytes_num; i++) {
        data[i] = (uint8_t)(i % 16);
    }

    int inode = 2;  // value will be overriden
    int lens[] = {0, 16};
    int offsets[] = {0, 1, 8, 16};
    int num_lens = sizeof(lens) / sizeof(lens[0]);
    int num_offsets = sizeof(offsets) / sizeof(offsets[0]);

    char *disk_name = "disk_img.2";
    //format(disk_name, 45);
    print_info("Mounting", "%s", disk_name);
    mount(disk_name);

    // Create a file to write to
    /*
    inode = create();
    if (inode < 0) {
        print_error("Failed to create file", "%d", inode);
        free(data);
        unmount();
        return;
    }
    print_success("Created file with inode", "%d", inode);
    */

    for (int l = 0; l < num_lens; l++) {
        for (int o = 0; o < num_offsets; o++) {
            int len = lens[l];
            int offset = offsets[o];

            print_info("Writing parameters", NULL);
            print_info("inode: ", "%d", inode);
            print_info("len: ", "%d", len);
            print_info("offset: ", "%d", offset);

            int bytes = write(inode, data, len, offset);
            if (bytes >= 0)
                print_success("Number of bytes successfully written", "%d", bytes);
            else
                print_error("Error when writing", "%d", bytes);

            // Optionally, read back and verify
            uint8_t *verify = malloc(len);
            if (verify) {
                int read_bytes = read(inode, verify, len, offset);
                print_info("Reading data:", NULL);
                for (int vi = 0; vi < read_bytes; vi++) {
                    fprintf(stdout, "%02X", verify[vi]);
                }
                fprintf(stdout, "\n");

                if (read_bytes == len && memcmp(data, verify, len) == 0) {
                    print_success("Verification passed", NULL);
                } else {
                    print_error("Verification failed", NULL);
                }
                memset(verify, 0, len);
                free(verify);
            }
            verify = malloc(len + offset);
            if (verify) {
                int read_bytes = read(inode, verify, len + offset, 0);
                print_info("Reading file from 0:", NULL);
                for (int vi = 0; vi < read_bytes; vi++) {
                    fprintf(stdout, "%02X", verify[vi]);
                }
                fprintf(stdout, "\n");
                memset(verify, 0, len + offset);
                free(verify);
            }
        }
    }

    print_info("Unmounting...", NULL);
    free(data);
    unmount();
}

/**
 * @brief Tests helper functions: get_free_block, set_data_block_pointer,
 *        get_data_block_pointer, and extend_file.
 */
void test4() {
    print_warning("Starting test4...", NULL);

    // Allocate a small buffer for reading/writing data
    int bytes_num = VDISK_SECTOR_SIZE;  // 1024 bytes
    uint8_t *data = malloc(bytes_num);
    if (!data) {
        print_error("Memory allocation failed", NULL);
        return;
    }
    memset(data, 0xAA, bytes_num);  // Fill with a pattern for write tests
    
    print_info("Allocating resources", "data is %d bytes", bytes_num);


    // Format and mount a new disk
    char *disk_name = "disk_img.4";
    int minimum_inodes = 32;
    print_info("Formatting", "%s with %d inodes", disk_name, minimum_inodes);
    int ret = format(disk_name, minimum_inodes);
    if (ret != 0) {
        print_error("Failed to format disk", "%d", ret);
        free(data);
        return;
    }

    print_info("Mounting", "%s", disk_name);
    ret = mount(disk_name);
    if (ret != 0) {
        print_error("Failed to mount disk", "%d", ret);
        free(data);
        return;
    }

    // Create a file for testing
    int inode_num = create();
    if (inode_num < 0) {
        print_error("Failed to create file", "%d", inode_num);
        free(data);
        unmount();
        return;
    }
    print_success("Created file with inode", "%d", inode_num);

    // Read inode block for testing
    uint8_t buffer[VDISK_SECTOR_SIZE];
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num = inode_num % 32;
    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        print_error("Failed to read inode block", "%d", ret);
        free(data);
        unmount();
        return;
    }
    inodes_block_t *ib = (inodes_block_t *)buffer;
    inode_t *target_inode = &ib[0][target_inode_num];

    // Test 1: get_free_block
    print_warning("Testing get_free_block", NULL);
    uint32_t block1, block2;
    ret = get_free_block(&block1);
    if (ret == 0) {
        print_success("Allocated block", "%u", block1); } 
    else {
        print_error("Failed to get first free block", "%d", ret); }
    
    ret = get_free_block(&block2);
    if (ret == 0) {
        print_success("Allocated block", "%u", block2); } 
    else {
        print_error("Failed to get second free block", "%d", ret); }

    // Test 2: set_data_block_pointer (direct, indirect, double-indirect)
    print_warning("Testing set_data_block_pointer", NULL);
    uint32_t logical_indices[] = {0, 3, 4, 260};  // Direct, last direct, first indirect, double-indirect
    int num_indices = sizeof(logical_indices) / sizeof(logical_indices[0]);
    for (int i = 0; i < num_indices; i++) {
        uint32_t logical = logical_indices[i];
        uint32_t physical;

        ret = get_free_block(&physical);
        if (ret == 0) {
            print_success("Allocated block", "%u", physical); } 
        else {
            print_error("Failed to get free block", "%d", ret); }

        ret = set_data_block_pointer(target_inode, logical, physical);
        if (ret == 0) {
            print_success("Set pointer blocks", "logical: %u, physical: %u", logical, physical); } 
        else {
            print_error("Failed to set pointer for logical block", "logical: %u, error code: %d", logical, ret); }
    }

    // Save inode after setting pointers
    ret = vdisk_write(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        print_error("Failed to save inode block", "%d", ret);
        free(data);
        unmount();
        return;
    }
    vdisk_sync(disk_handle);

    // Test 3: extend_file
    print_warning("Testing extend_file", NULL);
    uint32_t new_sizes[] = {1024, 4096, 5120, 266240};  // 1 block, 4 blocks (direct), 5 blocks (indirect), 260 blocks (double-indirect)
    int num_sizes = sizeof(new_sizes) / sizeof(new_sizes[0]);
    for (int i = 0; i < num_sizes; i++) {
        uint32_t new_size = new_sizes[i];
        ret = extend_file(target_inode, new_size);
        if (ret == 0) {
            print_success("Extended file to size", "%u", new_size);
            
            // Verify size
            if (target_inode->size == new_size) {
                print_success("Inode size updated correctly to", "%u", target_inode->size); }
            else {
                print_error("Inode size mismatch", "new_size: %u, target_inode->size: %u", new_size, target_inode->size); }

            // Verify blocks are allocated and zeroed
            ret = read(inode_num, data, VDISK_SECTOR_SIZE, new_size - VDISK_SECTOR_SIZE);
            if (ret >= 0) {
                int all_zeros = 1;
                for (int j = 0; j < ret; j++) {
                    if (data[j] != 0) {
                        all_zeros = 0;
                        break;
                    }
                }
                if (all_zeros) {
                    print_success("Last block is zeroed", "new_size: %u", new_size); } 
                else {
                    print_error("Last block is not zeroed", "new_size: %u", new_size); }
            } else {
                print_error("Failed to read last block", "new_size: %u, code: %d", new_size, ret);
            }
        } else {
            print_error("Failed to extend file", "new_size: %u, code: %d", new_size, ret);
        }

        // Save inode after extension
        ret = vdisk_write(disk_handle, 1 + target_inode_block, buffer);
        if (ret != 0) {
            print_error("Failed to save inode block", "%d", ret);
            break;
        }
        vdisk_sync(disk_handle);
    }

    // Test 4: Write and verify (uses extend_file and set_data_block_pointer indirectly)
    print_warning("Testing write (with extend_file)", NULL);
    int offset = 2048;
    ret = write(inode_num, data, VDISK_SECTOR_SIZE, offset);  // Write 1 block at offset 2048
    if (ret >= 0) {
        print_success("Wrote ", "%d", ret);
        // Read back to verify
        memset(data, 0, bytes_num);
        ret = read(inode_num, data, VDISK_SECTOR_SIZE, offset);
        if (ret >= 0) {
            int correct_data = 1;
            int i;
            for (i = 0; i < ret; i++) {
                if (data[i] != 0xAA) {
                    correct_data = 0;
                    break;
                }
            }
            if (correct_data) {
                print_success("Data verified", "offset: %d", offset);
            } else {
                print_error("Data incorrect", "offset: %d, i:", offset, i);
            }
        } else {
            print_error("Failed to read back data", "offset: %d, code: %d", offset, ret);
        }
    } else {
        print_error("Failed to write at offset 2048", "offset: %d, code: %d", offset, ret);
    }

    // Cleanup
    print_info("Unmounting...", NULL);
    free(data);
    unmount();
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






