#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "fs.h"
#include "vdisk.h"

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(int argc, char *argv[]) {

    // For custom scripts to test the program
    if (argc == 2 && strcmp(argv[1], "script") == 0) {
        
        char* disk = "disk_img_backup";
        mount(disk);

        int file_size = 2120318;
        
        stat(0);
        stat(1);
        stat(2);
        
        uint8_t data[file_size];
        read(1, data, file_size, 0);

        printf("Printing in the script :\n =========== \n");
        FILE *file = fopen("image.jpg", "r+w");
        if (file == NULL) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < file_size; i++) {
            fwrite(&data[i], sizeof(uint8_t), 1, file);
        }
        fclose(file);
        unmount();
        return 0;
    }

    return 0;
}