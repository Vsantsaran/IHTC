#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void func(char *);

void main (void) {
    int arr[] = {3, 2, 6, 4, 6, 44, 3, 2, 1, 4}, *b = arr+4;
    printf("%d", 2[arr]);
}

// void main (void) {
//     char *name, *ptr = "we are all humans and we must help each other with daily chores";
//     name = (char *) calloc (strlen(ptr), sizeof(char));
//     strcpy(name, ptr);
//     puts(ptr);
//     puts(name);
//     func(ptr);
//     // printf("size: %d\tsize: %d", strlen(ptr), strlen(name));
//     free(name);
// }

// void func(char *ptr) {
//     printf("size: %d", strlen(ptr));
// }
