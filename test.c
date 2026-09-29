#include <stdio.h>

// mul -> shl
int mul_pow2(int x) {
    return x * 8;
}

// mul -> lea
int mul_lea3(int x) {
    return x * 3;
}

// inc/dec
int inc_dec(int x) {
    x = x + 1;
    x = x - 1;
    return x;
}

// struct copy (movdqu)
typedef struct {
    long a, b, c, d;
    long e, f, g, h;
} Big;

void copy(Big *dst, Big *src) {
    *dst = *src;
}

// float zero cmp (pxor)
int fzero(float f) {
    if (f == 0.0f) return 1;
    return 0;
}

int main() {
    printf("%d\n", mul_pow2(3));   // 24
    printf("%d\n", mul_lea3(4));   // 12
    printf("%d\n", inc_dec(10));   // 10

    Big a = {1,2,3,4,5,6,7,8};
    Big b;
    copy(&b, &a);
    printf("%ld %ld\n", b.a, b.h); // 1 8

    printf("%d\n", fzero(0.0f));   // 1
    printf("%d\n", fzero(1.0f));   // 0
    return 0;
}
