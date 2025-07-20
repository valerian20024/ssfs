//! This file does blablabla

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

int read(int inode_num, uint8_t *data, int len, int offset) {
    printf("read called with inode_num=%d, data=%p, len=%d, offset=%d\n", inode_num, data, len, offset);
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];


    if (len == 0)
        return 0;

    /*if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto cleanup;
    }*/
    printf("vdisk read\n");
    ret = vdisk_read(disk_handle, 0, buffer);
    //! manage error

    superblock_t *sb = (superblock_t *)buffer;
    // Checking validity of the function parameter
    uint32_t total_inodes = sb->num_inode_blocks * 32;
    
    /*if (!is_inode_valid(inode_num, total_inodes)) {
        ret = ssfs_EALLOC;
        goto cleanup;
    }*/

    
    // Determining which precise inode we are looking for
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num   = inode_num % 32;

    printf("inode blocks\n target_inode_block : %d\n target_inode_num : %d\n", target_inode_block, target_inode_num);

    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    //! manage errors

    printf("casting buffer\n");
    inodes_block_t* ib = (inodes_block_t *)buffer;
    printf("allocating target inode\n");
    inode_t *target_inode = &ib[0][target_inode_num];

    //! check inode is valid.
    //! check offset + len < size of inode

    // Combien de db il faut (limite haute)
    // On copie tout dans un array de db
    // On commence à lire

    
    int required_data_blocks_num = 1 + (offset + len - 1) / 1024;
    printf("required_data_blocks_num : %d\n", required_data_blocks_num);

    uint32_t data_blocks_addresses[required_data_blocks_num];
    // fill the addresses

    get_file_block_addresses(target_inode, data_blocks_addresses, required_data_blocks_num);

    for (int i = 0; i < required_data_blocks_num; i++) {
        printf("data_blocks_addresses[%d] = %u\n", i, data_blocks_addresses[i]);
    }

//cleanup:
    if (ret != 0)
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


int write(int inode_num, uint8_t *data, int len, int offset) {
    int ret = 0;
    goto cleanup;
cleanup:
    return ret;
}
