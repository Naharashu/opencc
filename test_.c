#include <stdio.h>

#if __has_include(<stdio.h>)
int x = 1;
#else
int x = 0;
#endif

_Thread_local y = 2;

void main() {
	printf("%d\n", x);
};
