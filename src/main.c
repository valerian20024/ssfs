#include "ssfs_internal.h"

// if argc and argv not use, replace by "int main(void)" to suppress warnings at compilation
int main(void) {

    printf("my_var : %d", my_var);
    create();       // to 8
    printf("my_var : %d", my_var);
    unmount();      // to 7
    printf("my_var : %d", my_var);


    return 0;
}
