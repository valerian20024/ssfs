/*
 * ssfs_file_io.c
 * ==============
 * 
 * Manages data transfer to and from files within the file system.
 * Contains the read and write functions, along with their associated 
 * helper functions.
 * 
 * 
 */

#include <string.h>
#include <stdlib.h>

#include "fs.h"
#include "ssfs_internal.h"
#include "error.h"

/**
 */
/**
 * @brief Reads a specified number of bytes from a file at a given offset.
 *
 * This function reads `len` bytes from the file identified by `inode_num`,
 * starting at `offset` bytes from the beginning of the file. The read data
 * is then copied into the provided `data` buffer.
 * @note This buffer must be pre-allocated by the caller to be large
 * enough to hold `len` bytes.
 *
 * @param inode_num The inode number of the target file to read from.
 * @param data A pointer to the buffer where the read data will be stored.
 * @param len The maximum number of bytes to read into the `data` buffer.
 * @param offset The byte offset from the beginning of the file where reading should start.
 *
 * @return The number of bytes actually read and copied into the `data` buffer on success.
 * This value may be less than `len` if the end of the file is reached
 * before `len` bytes could be read.
 * @return A negative integer (error codes) on failure. 
 *
 * @note This function will fill portions of the `data` buffer with zeros if it encounters
 * file holes within the specified read range.
 * @note Won't test file reachability if reading 0 bytes.
 */


/*

!The calculation required_data_blocks_num = 1 + (offset + len - 1) / 1024 assumes you need enough blocks to cover offset + len. This is correct for determining the maximum number of blocks needed but could overestimate if the file size is smaller than offset + len. You should check the file size (likely stored in target_inode->size) to avoid accessing invalid blocks.
!The function doesn’t check if offset is negative or if offset + len exceeds the file size, which could lead to reading beyond the file’s data. This is noted in your comment about handling reads larger than the file size, but it’s not implemented.


*/

int read(int inode_num, uint8_t *data, int _len, int _offset) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Checking input parameters
    if (_len < 0 || _offset < 0) {
        ret = ssfs_EINVAL;
        goto error_management;
    }

    // Trivial empty reading
    if (_len == 0)
        return ret;

    if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto error_management;
    }

    uint32_t len = (uint32_t) _len;
    uint32_t offset = (uint32_t) _offset;  

    // Reading superblock
    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0)
        goto error_management;
    superblock_t *sb = (superblock_t *)buffer;

    // Checking inode validity
    uint32_t total_inodes = sb->num_inode_blocks * 32;
    if (!is_inode_valid(inode_num, total_inodes)) {
        ret = ssfs_EALLOC;
        goto error_management;
    }
    
    // Reading the inode block and finding the target inode
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num = inode_num % 32;
    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0)
        goto error_management;
    inodes_block_t* ib = (inodes_block_t *)buffer;
    inode_t *target_inode = &ib[0][target_inode_num];

    // Checking inode usage
    if (!target_inode->valid) {
        ret = ssfs_EINODE;
        goto error_management;
    }

    // Checking file size
    if (offset >= target_inode->size) {
        ret = ssfs_EREAD;  // Nothing to read if offset is beyond file size
        goto error_management;
    }
    if (offset + len > target_inode->size)
        len = target_inode->size - offset;  // Read only available data

    // data_block_addresses will hold the addresses of data blocks we need to look for
    uint32_t required_data_blocks_num = 1 + (offset + len - 1) / VDISK_SECTOR_SIZE;
    uint32_t *data_block_addresses = malloc(required_data_blocks_num * sizeof(uint32_t));
    if (data_block_addresses == NULL) {
        ret = ssfs_EALLOC;
        goto error_management;
    }
    ret = get_file_block_addresses(target_inode, data_block_addresses);
    if (ret != 0)
        goto error_management;

    // Read data blocks
    uint32_t bytes_read = 0;
    while (bytes_read < len) {
        uint32_t absolute_file_position = offset + bytes_read;
        uint32_t block_index = absolute_file_position / VDISK_SECTOR_SIZE;
        uint32_t offset_within_block = absolute_file_position % VDISK_SECTOR_SIZE;

        // Will copy the minimum between the remaining bytes in the block and in total
        uint32_t bytes_remaining_in_block = VDISK_SECTOR_SIZE - offset_within_block;
        uint32_t bytes_remaining_in_total = len - bytes_read;
        uint32_t bytes_to_read = (bytes_remaining_in_block < bytes_remaining_in_total) ?
            bytes_remaining_in_block :
            bytes_remaining_in_total;
        
        ret = vdisk_read(disk_handle, data_block_addresses[block_index], buffer);
        if (ret != 0)
            goto error_management;

        memcpy(data + bytes_read, buffer + offset_within_block, bytes_to_read);

        bytes_read += bytes_to_read;
        offset_within_block = 0;
    }

    free(data_block_addresses);
    return (int)bytes_read;

error_management:
    free(data_block_addresses);
    fprintf(stderr, "Error when reading (code %d).\n", ret);
    return ret;
}

/**
 * @brief This function will write all the addresses of data blocks *used*
 * by a file into address_buffer.
 * @return 0 on success. Writes the data into address_buffer.
 * @return Error codes on failure.
 * @note It assumes the address_buffer is correctly sized. It assumes the caller
 * function will deal with the error codes.
 */
int get_file_block_addresses(inode_t *inode, uint32_t *address_buffer) {
    int ret = 0;
    int addresses_collected = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Looking for direct addresses
    for (int d = 0; d < 4; d++) {
        if (inode->direct[d]) {
            address_buffer[addresses_collected++] = inode->direct[d];
        }
    }

    // Looking for indirect addresses and related
    if (inode->indirect1) {
        ret = vdisk_read(disk_handle, inode->indirect1, buffer);
        if (ret != 0)
            return ret;

        uint32_t *indirect_ptrs = (uint32_t *)buffer;

        for (int db = 0; db < 256; db++) {
            if (indirect_ptrs[db]) {
                address_buffer[addresses_collected++] = indirect_ptrs[db];
            }
        }
    }

    // Looking for double indirect addresses and related
    if (inode->indirect2) {
        ret = vdisk_read(disk_handle, inode->indirect2, buffer);
        if (ret != 0)
            return ret;

        uint32_t *double_indirect_ptrs = (uint32_t *)buffer;

        uint8_t indirect_pointers_buffer[VDISK_SECTOR_SIZE];
        for (int ip = 0; ip < 256; ip++) {
            if (double_indirect_ptrs[ip]) {
                ret = vdisk_read(disk_handle, double_indirect_ptrs[ip], indirect_pointers_buffer);  // !pb here
                if (ret != 0)
                    return ret;

                uint32_t *indirect_ptrs = (uint32_t *)indirect_pointers_buffer;

                for (int db = 0; db < 256; db++) {
                    if (indirect_ptrs[db]) {
                        address_buffer[addresses_collected++] = indirect_ptrs[db];
                    }
                }
            }
        }
    }

    return ret;
}

/**
 * @brief Writes a specified number of bytes to a file at a given offset.
 *
 * This function writes `len` bytes from the `data` buffer into the file
 * identified by `inode_num`, starting at `offset` bytes from the beginning
 * of the file.
 *
 * The function handles file expansion if the write operation extends beyond
 * the current file size. If the write creates a gap (i.e., `offset` is
 * greater than the current file size), this gap will be implicitly filled
 * with zeros by allocating new blocks as needed. 
 * 
 * @note The function will also try to infer certain mecanisms if bad input are
 * given. That is, if one tries to write from *before* the beginning to *after*
 * the end of the file, the function will shift the indexes to write correctly 
 * over and beyond the file.
 *
 * @param inode_num The inode number of the target file.
 * @param data A pointer to the buffer containing the data to be written.
 * @param len The number of bytes to write from the `data` buffer.
 * @param offset The byte offset from the beginning of the file where writing should start.
 *
 * @return The number of bytes actually written from the `data` buffer on success.
 * (Note: Bytes used to fill implicit gaps with zeros are *not* counted in this return value).
 * @return A negative integer (error codes) on failure.
 *
 */
int write(int inode_num, uint8_t *data, int _len, int _offset) {
    uint32_t len = (uint32_t) _len;
    uint32_t offset = (uint32_t) _offset;

    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    // Determining which precise inode we are looking for
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num   = inode_num % 32;

    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }

    inodes_block_t* ib = (inodes_block_t *)buffer;
    inode_t *target_inode = &ib[0][target_inode_num];

    // Case 1.1 : writing nothing, which trivially is a success.
    if (len == 0)
        return 0;

    // Case 1.2 without Case 6 : Writing before the beginning of file 
    // up to a certain point *in* the file. Non-guessable behavior, so
    // it's an error. 
    if (_offset < 0 && (-_offset) + len < target_inode->size)
        return 0;

    // Case 2 : Writing from the beginning of (or from inside) the file
    // to a point in the file. Basic behavior.  
    if (offset <= target_inode->size && offset + len > target_inode->size)
        write_in_file(target_inode, data, len, offset);
    
    // Case 3 : Writing from inside the file and goind beyond. Must
    // first write inside the file, then complete with new allocation 
    // and writes.
    if (offset <= target_inode->size && offset + len > target_inode->size) {
        //write_in_file();
        //write_out_file();
    }

    // Case 4 : Writing only past the end of the file. Requires new
    // allocation, but we don't need to write inside the file.
    if (offset >= target_inode->size) 
        //write_out_file();

    // Case 5 : If one wants to write something bigger than the actual file
    // even if it wanted to start before it, the best to infer from that
    // is to rewrite over the file and complete by allocating new blocks.
    if (len > target_inode->size) {
        //We need to tweak the numbers.
    }
    


    /*
    
    1 - len = 0 => OK return
      - offset < 0 => error

    2 - offset >= 0 && offset + len <= size : on ecrit dans le fichier sans allocation supplementaire

    3 - offset <= size et offset + len > size : on ecrit dans le fichier et on alloue en plus et on écrit dedans
        - allouer combien de blocks ? Retrouver leur valeur, ecrire dedans. ecrire combien de bytes dedans ?
    4.1 - offset = size : alors on ne doit pas ecrire dans le fichier mais allouer des blocs et ecrire dedans 
        - allouer combien de blocks ? Retrouver leur valeur, ecrire dedans. ecrire combien de bytes dedans ?
    4.2 - offset > size : on ne doit pas ecrire dans le fichier, mais on alloue, on ecrit des zeros et on ecrit dedans
        - allouer combien de blocks ? Retrouver leur valeur, ecrire dedans. ecrire combien de bytes dedans et apres combien de zeros ?
    6 - len > size, alors on fait un tour de passe passe, et on retombe sur le cas 4 avec offset = 0, et on a calculé jusqu'où aller;

    -> Il faut une fonction qui ecrit dans le fichier de base, sans allouer. On lui passe les valeurs 
    qu'il faut pour qu'elle fasse son travail.

    -> Il faut un fonction qui va allouer des blocks et ecrire dedans ce qu'il faut.
    "allouer" ça veut dire flip un bit dans la bitmap ET changer les addresses de direct, ind, ind2 dans l'inode.
    Il faut aussi penser à changer la taille du fichier dans l'inode.

    */

    goto error_management;
error_management:
    return ret;
}

/**
 * This function will write only inside an existing file
 * It returns the number of bytes actually written to the file on success; error codes on failure.
 */
int write_in_file(inode_t *inode, uint8_t *data, uint32_t len, uint32_t offset) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];  // Will allow to read to and write from

    // * no need to check inode's fields validity, it must be done by the caller fn
 
    // Computing the total number of DB for the file
    uint32_t required_data_blocks_num = (inode->size + VDISK_SECTOR_SIZE - 1) / VDISK_SECTOR_SIZE;
    uint32_t *data_block_addresses = malloc(required_data_blocks_num * sizeof(uint32_t));
    //! error

    get_file_block_addresses(inode, data_block_addresses);
    
    uint32_t bytes_written = 0;
    while (bytes_written < len) {

        // This is the absolute position within the file
        uint32_t absolute_file_position = offset + bytes_written;
        
        // The block we're currently positioned and the byte in that block
        // block_index is the logical block index, i.e. the index in data_block_addresses
        // which is different from the actual physical block index in the disk
        uint32_t block_index = absolute_file_position / VDISK_SECTOR_SIZE;
        uint32_t offset_within_block = absolute_file_position % VDISK_SECTOR_SIZE;

        // Will copy the minimum between the remaining bytes in the block and in total
        uint32_t bytes_remaining_in_block = VDISK_SECTOR_SIZE - offset_within_block;
        uint32_t bytes_remaining_in_total = len - bytes_written;
        uint32_t bytes_to_write = (bytes_remaining_in_block < bytes_remaining_in_total) ?
            bytes_remaining_in_block :
            bytes_remaining_in_total;
     
        // Read, change, write, sync
        ret = vdisk_read(disk_handle, data_block_addresses[block_index], buffer);
        if (ret != 0)
            goto error_management;

        // Now we write in the buffer
        memcpy(buffer + offset_within_block, data + bytes_written, bytes_to_write);

        // Write back to the disk and sync
        vdisk_write(disk_handle, data_block_addresses[block_index], buffer);
        //! errors        
        vdisk_sync(disk_handle);

        // Updating variables for next iteration
        bytes_written += bytes_to_write;
        offset_within_block = 0;  // When block 0 is written, we write from the beginning
    }

    free(data_block_addresses);
    return bytes_written;

error_management:
    free(data_block_addresses);
    return ret;
}

/**
 * @brief This function will take care of allocating new blocks to file and write data to them.
 * 
 * It will: 
 * - Set a bit to 'in use' in the bitmap representing physical blocks 
 * - Add new fields to direct pointers, indirect pointers, double indirect pointers in the inode.
 * - Change the size of the file in the inode.
 */
int write_out_file(inode_t *inode, uint8_t *data, uint32_t len, uint32_t offset) {
    (void)inode;
    (void)data;
    (void)len;
    (void)offset;
    return 0;
}


