#include "ssfs_internal.h"
#include "fs.h"
#include <stdbool.h>

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(void) {

    bool bitmap[11] = {
        false, false, false, false,
        false, false, false, false,
        false, false, false
    };

    allocated_block(bitmap, 0);
    allocated_block(bitmap, 4);
    allocated_block(bitmap, 10);

    for (int i = 0; i < 11; i++) {
        printf("%d, ", bitmap[i]);
    }

}
