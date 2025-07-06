#include <stdio.h>

void main(void) {
	int a[4] = { 1, 2, 3, 4 }, **p = &a, ***q = p;
	for (int i=0; i<4; ++i) printf("%d\t", a[i]);
	printf("\n\nsizeof(p) = %d\nsizeof(a) = %d\nsizeof(q) = %d", sizeof(p), sizeof(a), sizeof(q));
}