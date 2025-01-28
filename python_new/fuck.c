#include <stdio.h>
#include <conio.h>
#include <values.h>

int main (void) {
    printf ("Max int = %d", __INT32_MAX__);
	printf ("\nMax long = %lld", __LONG_LONG_MAX__);
	printf ("\nMin float = %f", __FLT_MIN__);
	printf ("\nMax float = %g", __FLT_MAX__);
	printf ("\nMin double = %Lf", __DBL_MIN__);
	printf ("\nMax double = %lg", __DBL_MAX__);
	// getch();
	return 0;
}
