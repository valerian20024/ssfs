//! This file does blablabla

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

#include <string.h>

/**
 * @note Won't test file reachability if reading 0 bytes.
 */
int read(int inode_num, uint8_t *data, int len, int offset) {
    printf("read called with inode_num=%d, data=%p, len=%d, offset=%d\n", inode_num, data, len, offset);
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];
    
    uint32_t _len = (uint32_t) len;
    uint32_t _offset = (uint32_t) offset;

    // Trivial empty reading
    if (_len == 0) {
        return 0;
    }
    
    // This buffer will hold the addresses of all the data blocks we need to look
    // It is initialized early so that we can use the goto instructions.
    uint32_t required_data_blocks_num = 1 + (_offset + _len - 1) / 1024;
    printf("required_data_blocks_num : %d\n", required_data_blocks_num);
    uint32_t data_blocks_addresses[required_data_blocks_num];  //todo use malloc instead of the stack

    
        
    if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto cleanup;
    }

    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup;
    }
    superblock_t *sb = (superblock_t *)buffer;

    // Checking validity of the function parameter
    uint32_t total_inodes = sb->num_inode_blocks * 32;
    if (!is_inode_valid(inode_num, total_inodes)) {
        ret = ssfs_EALLOC;
        goto cleanup;
    }

    
    // Determining which precise inode we are looking for
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num   = inode_num % 32;

    printf("inode blocks\n target_inode_block : %d\n target_inode_num : %d\n", target_inode_block, target_inode_num);

    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto cleanup;
    }

    inodes_block_t* ib = (inodes_block_t *)buffer;
    inode_t *target_inode = &ib[0][target_inode_num];

    // Checking validity of inode
    if (!target_inode->valid) {
        ret = ssfs_EINODE;
        goto cleanup;
    }

    // Read request must be smaller than the file 
    if (_offset + _len > target_inode->size) {
        ret = ssfs_EREAD;
        goto cleanup;
    }

    // Let's get the addresses we need and fill in the buffer
    get_file_block_addresses(target_inode, data_blocks_addresses, required_data_blocks_num);

    for (uint32_t i = 0; i < required_data_blocks_num; i++) {
        printf("data_blocks_addresses[%d] = %u\n", i, data_blocks_addresses[i]);
    }

    // --- Main loop for reading ---
    uint32_t bytes_read = 0;
    // Targets the start of the bytes we need to copy in the block.
    // It starts with _offset for the first block, then it gets updated 
    // to zero to start at the beginning of further blocks.
    uint32_t offset_within_block = _offset;  

    // todo check when offset is greater than the size of one block (eg we start at 3rd block)
    // todo will need to use modulo right above ^^^ Also check get_file_block_addr()

    while (bytes_read < _len) {
        // For each data block address
        for (uint32_t addr_num = 0; addr_num < required_data_blocks_num; addr_num++) {
            // Put in buffer the data block content
            vdisk_read(disk_handle, data_blocks_addresses[addr_num], buffer);
            //! error

            // Need to copy from the current block the minium between:
            // 1. Bytes remaining in the block.
            // 2. Bytes remaining to fulfill the '_len' request.
            uint32_t bytes_remaining_in_block = VDISK_SECTOR_SIZE - offset_within_block;
            uint32_t bytes_remaining_in_total = _len - bytes_read;

            uint32_t bytes_to_copy = (bytes_remaining_in_block < bytes_remaining_in_total) ?
                bytes_remaining_in_block :
                bytes_remaining_in_total;

            memcpy(data + bytes_read, buffer, bytes_to_copy);
            bytes_read += bytes_to_copy;

            offset_within_block = 0;
        }
    }

    ret = (int)bytes_read;


cleanup:
    if (ret < 0)
        fprintf(stderr, "Error when reading (code %d).\n", ret);
    return ret;
}


int get_file_block_addresses(inode_t *inode, uint32_t *address_buffer, int max_addresses) {

    //! need to stop avec max_addresses
    int ret = 0;
    printf("get_file_block_addresses called with inode=%p, address_buffer=%p, max_addresses=%d\n", inode, address_buffer, max_addresses);

    int addresses_collected = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    for (int d = 0; d < 4; d++) {
        if (inode->direct[d] && addresses_collected < max_addresses) {
            address_buffer[addresses_collected++] = inode->direct[d];
            printf("direct : address_collected : %d\n", addresses_collected);
        }
    }

    if (inode->indirect1) {
        vdisk_read(disk_handle, inode->indirect1, buffer);
        //! manage errprs
        uint32_t *indirect_ptrs = (uint32_t *)buffer;

        for (int db = 0; db < 256; db++) {
            if (indirect_ptrs[db] && addresses_collected < max_addresses) {
                address_buffer[addresses_collected++] = indirect_ptrs[db];
                printf("indirect : address_collected : %d\n", addresses_collected);
            }
        }
    }

    if (inode->indirect2) {
        vdisk_read(disk_handle, inode->indirect2, buffer);
        //! manage errprs
        uint32_t *double_indirect_ptrs = (uint32_t *)buffer;

        uint8_t indirect_pointers_buffer[VDISK_SECTOR_SIZE];
        for (int ip = 0; ip < 256; ip++) {
            if (double_indirect_ptrs[ip]) {
                vdisk_read(disk_handle, double_indirect_ptrs[ip], indirect_pointers_buffer);
                //! errors
                uint32_t *indirect_ptrs = (uint32_t *)indirect_pointers_buffer;

                for (int db = 0; db < 256; db++) {
                    if (indirect_ptrs[db] && addresses_collected < max_addresses) {
                        address_buffer[addresses_collected++] = indirect_ptrs[db];
                        printf("double indirect : address_collected : %d\n", addresses_collected);
                    }
                }
            }
        }
    }

    return ret;
}


/*int write(int inode_num, uint8_t *data, int len, int offset) {

//! cast len and offset cause it sucks
    int ret = 0;
    goto cleanup;
cleanup:
    return ret;
}*/
