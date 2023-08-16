#include <stdio.h>

int testfunc1() {
    printf("testfunc1 called!\n");
    return 3;
}
int testfunc2(int a, int b) {
    printf("testfunc2,%d,%d called!\n", a, b);
    return a+b;
}
