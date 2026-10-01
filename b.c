#include <stdio.h>

int main() {
	#ifdef __SSE2__
	printf("SSE2 is available!\n");
	#endif	

	#ifdef __SSE3__
	printf("SSE3 is available!\n");
	#endif

	#ifdef __SSSE3__
	printf("SSSE3 is available!\n");
	#endif

	#ifdef __SSE4_2__
	printf("SSE4.2 is available!\n");
	#endif

	#ifdef __AVX__
	printf("AVX is available!\n");
	#endif

	#ifdef __AVX2__
	printf("AVX2 is available!\n");
	#endif

	#ifdef __AVX512F__
	printf("AVX512F is available!\n");
	#endif

	return 0;
}
