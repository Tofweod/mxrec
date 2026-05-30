#ifndef TOF_MXREC_RANDOM_H
#define TOF_MXREC_RANDOM_H

#include "stddef.h"

enum distribution_type {
	DISCUNIFORM_DISTRIBUTION, // [a,b)
	CONTUNIFORM_DISTRIBUTION, // [a,b]
	// TODO
	BINO_DISTRIBUTION,
	EXP_DISTRIBUTION,     // Exp(λ)
	NORMAL_DISTRIBUTION,  // N(μ,σ²)
	POSSION_DISTRIBUTION, // Possin(λ)
};

// exposed
int uniform01(double *out);
int uniform01Array(double *out, size_t n);
#define getRandomArray(size, res, type, ...) __getRandomArray(size, res, type, __VA_ARGS__)

int __getRandomArray(size_t n, void *res, enum distribution_type type, ...);
#endif
