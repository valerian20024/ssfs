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
int read(int inode_num, uint8_t *data, int _len, int _offset) {
    int ret = 0;
    uint8_t buffer[VDISK_SECTOR_SIZE];

    uint32_t len = (uint32_t) _len;
    uint32_t offset = (uint32_t) _offset;

    // Trivial empty reading
    if (len == 0)
        return ret;

    // This buffer will hold the addresses of all the data blocks we need to look
    // It is initialized early so that we can use the goto instructions.
    uint32_t required_data_blocks_num = 1 + (offset + len - 1) / 1024;
    printf("required_data_blocks_num : %d\n", required_data_blocks_num);
    uint32_t data_blocks_addresses[required_data_blocks_num];  //todo use malloc instead of the stack
    
    if (!is_mounted()) {
        ret = ssfs_EMOUNT;
        goto error_management;
    }

    ret = vdisk_read(disk_handle, 0, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }
    superblock_t *sb = (superblock_t *)buffer;

    // Checking validity of the function parameter
    uint32_t total_inodes = sb->num_inode_blocks * 32;
    if (!is_inode_valid(inode_num, total_inodes)) {
        ret = ssfs_EALLOC;
        goto error_management;
    }
    
    // Determining which precise inode we are looking for
    uint32_t target_inode_block = inode_num / 32;
    uint32_t target_inode_num   = inode_num % 32;

    printf("inode blocks\n target_inode_block : %d\n target_inode_num : %d\n", target_inode_block, target_inode_num);

    ret = vdisk_read(disk_handle, 1 + target_inode_block, buffer);
    if (ret != 0) {
        ret = vdisk_EACCESS;
        goto error_management;
    }

    inodes_block_t* ib = (inodes_block_t *)buffer;
    inode_t *target_inode = &ib[0][target_inode_num];

    // Checking validity of inode
    if (!target_inode->valid) {
        ret = ssfs_EINODE;
        goto error_management;
    }

    // Read request must be smaller than the file 
    if (offset + len > target_inode->size) {
        ret = ssfs_EREAD;
        goto error_management;
    }

    // Let's get all the addresses of datablocks and fill in the buffer
    ret = get_file_block_addresses(target_inode, data_blocks_addresses);
    if (ret != 0)
        goto error_management;

    uint32_t bytes_read = 0;
    // Targets the start of the bytes we need to copy in the block.
    // It starts with _offset for the first block, then it gets updated 
    // to zero to start at the beginning of further blocks.
    uint32_t offset_within_block = offset;  

    // todo check when offset is greater than the size of one block (eg we start at 3rd block)
    // todo will need to use modulo right above ^^^ Also check get_file_block_addr()
    //* See write function

    // For each data block address
    for (uint32_t addr_num = 0; addr_num < required_data_blocks_num; addr_num++) {

        if (bytes_read >= len)
            break;
        // Put in buffer the data block content
        vdisk_read(disk_handle, data_blocks_addresses[addr_num], buffer);
        //! error

        // Need to copy from the current block the minium between:
        // 1. Bytes remaining in the block.
        // 2. Bytes remaining to fulfill the '_len' request.
        uint32_t bytes_remaining_in_block = VDISK_SECTOR_SIZE - offset_within_block;
        uint32_t bytes_remaining_in_total = len - bytes_read;

        uint32_t bytes_to_copy = (bytes_remaining_in_block < bytes_remaining_in_total) ?
            bytes_remaining_in_block :
            bytes_remaining_in_total;

        // todo check this ... Might be why data is full of zeros...
        // When addr_num = 1, any of the two functions would change the value of *disk_handle 
        //memcpy(data + bytes_read, buffer, bytes_to_copy);
        //memset(data + bytes_read, buffer, bytes_to_copy);

        bytes_read += bytes_to_copy;

        offset_within_block = 0;
    }

    return (int)bytes_read;

error_management:
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
                ret = vdisk_read(disk_handle, double_indirect_ptrs[ip], indirect_pointers_buffer);
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
    uint8_t iobuffer[VDISK_SECTOR_SIZE];  // Will allow to read to and write from

    // * no need to check inode's fields validity, it must be done by the caller fn
 
    // Computing the total number of DB for the file
    uint32_t required_data_blocks_num = (inode->size + VDISK_SECTOR_SIZE - 1) / VDISK_SECTOR_SIZE;
    uint32_t *data_block_addresses = malloc(required_data_blocks_num * sizeof(uint32_t));
    //! error

    get_file_block_addresses(inode, data_block_addresses);
    
    uint32_t bytes_written = 0;
    while (bytes_written < len) {

        // This is the absolute position within the file
        uint32_t current_file_position = offset + bytes_written;
        
        // The block we're currently positioned and the byte in that block
        // block_index is the logical block index, i.e. the index in data_block_addresses
        // which is different from the actual physical block index in the disk
        uint32_t block_index = current_file_position / VDISK_SECTOR_SIZE;
        uint32_t offset_within_block = current_file_position % VDISK_SECTOR_SIZE;

        // Will copy the minimum between the two
        uint32_t bytes_remaining_in_block = VDISK_SECTOR_SIZE - offset_within_block;
        uint32_t bytes_remaining_in_total = len - bytes_written;
        uint32_t bytes_to_write = (bytes_remaining_in_block < bytes_remaining_in_total) ?
            bytes_remaining_in_block :
            bytes_remaining_in_total;
     
        // Read, change, write, sync
        vdisk_read(disk_handle, data_block_addresses[block_index], iobuffer);
        //! errors

        // Now we write in the buffer
        memcpy(iobuffer + offset_within_block, data + bytes_written, bytes_to_write);

        // Write back to the disk and sync
        vdisk_write(disk_handle, data_block_addresses[block_index], iobuffer);
        //! errors        
        vdisk_sync(disk_handle);

        // Updating variables for next iteration
        bytes_written += bytes_to_write;
        offset_within_block = 0;  // When block 0 is written, we write from the beginning
    }

    free(data_block_addresses);
    return bytes_written;
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
    return 0;
}


