#include "myalloc.h"
#include <stdio.h>

// A simple test program to show the  use of the library
// By default, myalloc() simply calls malloc() so this will
// work.
// Kasim Terzic, Sep 2017

void main()
{
    printf("Allocating 1024 * sizeof(int)... \n");

    printf("%lu\n", sizeof(size_t));

    int* ptr = myalloc(1024*sizeof(int));
    printf("%p\n", ptr);
    ptr[0] = 42;

    printf("Freeing the allocated memory... \n");
    myfree(ptr);

    printf("Yay!\n");
}
