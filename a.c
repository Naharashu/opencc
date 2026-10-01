#include <stdio.h>
int main() {
  printf("%d %d %d\n", __builtin_popcount(0xff00ff), __builtin_clz(1), __builtin_ctz(8));
  printf("%d %d %d\n", __builtin_popcountll(-1L), __builtin_clzll(1), __builtin_ctzll(1UL << 40));
}

