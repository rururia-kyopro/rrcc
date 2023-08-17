#include <stdio.h>

int testfunc1() {
    printf("testfunc1 called!\n");
    return 3;
}
int testfunc2(int a, int b) {
    printf("testfunc2,%d,%d called!\n", a, b);
    return a+b;
}
int testfunc3(int *a) {
    printf("testfunc3,%d,%d called!\n", a[0], a[1]);
    return a[0]+a[1];
}
