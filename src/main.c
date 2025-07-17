#include "ssfs_internal.h"
#include "fs.h"

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(void) {

    char* c = "testdisk.img";
    int ret = format(c, 8);

    return ret;
}
