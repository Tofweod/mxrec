#include "random.h"
#include <assert.h>
#include <stdio.h>

int main()
{
	size_t i;

	double r1;
	if (uniform01(&r1) == 0) {
		printf("uniform01 is %lf\n", r1);
	}

#define ARRAYSIZE 50
	double r2[ARRAYSIZE];
	if (uniform01Array(r2, ARRAYSIZE) == 0) {
		printf("uniform01Array is:\n");
		for (i = 0; i < ARRAYSIZE; ++i) {
			printf("%lf ", r2[i]);
		}
		printf("\n");
	}

	long long r3[ARRAYSIZE];
	if (getRandomArray(ARRAYSIZE, r3, DISCUNIFORM_DISTRIBUTION, 100, 200) == 0) {
		printf("discrete uniform distribution is:\n");
		for (i = 0; i < ARRAYSIZE; ++i) {
			printf("%lld  ", r3[i]);
			assert(100 <= r3[i] && r3[i] < 200);
		}
		printf("\n");
	}

	double r4[ARRAYSIZE];
	if (getRandomArray(ARRAYSIZE, r4, CONTUNIFORM_DISTRIBUTION, 100.0f, 200.0f) == 0) {
		printf("continuous uniform distribution is:\n");
		for (i = 0; i < ARRAYSIZE; ++i) {
			printf("%lf ", r4[i]);
			assert(100 <= r4[i] && r4[i] <= 200);
		}
		printf("\n");
	}

	double r5[ARRAYSIZE];
	if (getRandomArray(ARRAYSIZE, r5, NORMAL_DISTRIBUTION, 100.0f, 5.0f) == 0) {
		printf("normalization distribution is:\n");
		for (i = 0; i < ARRAYSIZE; ++i) {
			printf("%lf ", r5[i]);
		}
		printf("\n");
	}

	double r6[ARRAYSIZE];
	if (getRandomArray(ARRAYSIZE, r6, EXP_DISTRIBUTION, 10.0f) == 0) {
		printf("exponential distribution is:\n");
		for (i = 0; i < ARRAYSIZE; ++i) {
			printf("%lf ", r6[i]);
		}
		printf("\n");
	}

	unsigned long long r7[ARRAYSIZE];
	if (getRandomArray(ARRAYSIZE, r7, POSSION_DISTRIBUTION, 10.0) == 0) {
		printf("possion distribution is:\n");
		for (i = 0; i < ARRAYSIZE; ++i) {
			printf("%lld ", r7[i]);
		}
		printf("\n");
	}
	return 0;
}
