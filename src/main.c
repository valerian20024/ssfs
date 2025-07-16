#include "ssfs_internal.h"

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(void) {

    char* c = 4;
    int ret = format(c, 8);

    printf("ret: %d", ret);
    return 0;
}
