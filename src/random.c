#include "random.h"
#include "xmalloc.h"
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <sys/random.h>

int uniform01(double *out)
{
	if (out == NULL)
		return -1;
	unsigned int x;
	if (getrandom(&x, sizeof(x), 0) != sizeof(x))
		return -1;
	*out = (double)x / ((double)UINT_MAX + 1.0);
	return 0;
}

int uniform01Array(double *out, size_t n)
{
	if (out == NULL)
		return -1;
	unsigned int *raw;
	size_t i, raw_size = n * sizeof(*raw);
	raw = xmalloc(raw_size);
	if (getrandom(raw, raw_size, 0) != raw_size) {
		xfree(raw);
		return -1;
	}
	for (i = 0; i < n; ++i)
		out[i] = (double)raw[i] / ((double)UINT_MAX + 1.0);
	xfree(raw);
	return 0;
}

static int gen_uniform_discrete(long long *res, size_t n, long long a, long long b)
{
	if (a >= b)
		return -1;
	double *raw;
	size_t i;

	raw = xmalloc(n * sizeof(*raw));
	if (uniform01Array(raw, n)) {
		xfree(raw);
		return -1;
	}
	for (i = 0; i < n; ++i) {
		res[i] = a + (long long)(raw[i] * (b - a));
	}
	xfree(raw);
	return 0;
}

static int gen_uniform_continuous(double *res, size_t n, double a, double b)
{
	if (a >= b)
		return -1;
	double *raw;
	size_t i;
	raw = xmalloc(n * sizeof(*raw));
	if (uniform01Array(raw, n)) {
		xfree(raw);
		return -1;
	}
	for (i = 0; i < n; ++i) {
		res[i] = a + (b - a) * raw[i];
	}
	xfree(raw);
	return 0;
}

static int gen_normal(double *res, size_t n, double mu, double sigma)
{
	size_t i, j, raw_size = 2 * ((n + 1) / 2);
	double *raw = xmalloc(raw_size * sizeof(double));
	if (uniform01Array(raw, raw_size)) {
		xfree(raw);
		return -1;
	}

	for (i = 0, j = 0; j + 1 < raw_size; j += 2) {
		double u1 = raw[j];
		double u2 = raw[j + 1];
		double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
		double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
		res[i++] = mu + sigma * z0;
		if (i < n)
			res[i++] = mu + sigma * z1;
	}
	xfree(raw);
	return 0;
}

static int gen_exponential(double *res, size_t n, double lambda)
{
	size_t i;
	double *raw = xmalloc(n * sizeof(double));
	if (uniform01Array(raw, n)) {
		xfree(raw);
		return -1;
	}
	for (i = 0; i < n; ++i) {
		res[i] = -log(1.0 - raw[i]) / lambda;
	}
	xfree(raw);
	return 0;
}

static int gen_possion(unsigned long long *res, size_t n, double lambda)
{
	// Knuth
	size_t i;
	int k;
	double tmp, p, L;
	L = exp(-lambda);
	for (i = 0; i < n; ++i) {
		k = 0;
		p = 1.0;
		do {
			if (uniform01(&tmp))
				return -1;
			p *= tmp;
			++k;
		} while (p >= L);
		res[i] = k - 1;
	}
	return 0;
}

int __getRandomArray(size_t n, void *res, enum distribution_type type, ...)
{
	if (res == NULL)
		return -1;

	va_list args;
	va_start(args, type);

	int ret = 0;
	switch (type) {
	case DISCUNIFORM_DISTRIBUTION: {
		long long a = va_arg(args, long long);
		long long b = va_arg(args, long long);
		ret = gen_uniform_discrete(res, n, a, b);
		break;
	}
	case CONTUNIFORM_DISTRIBUTION: {
		double a = va_arg(args, double);
		double b = va_arg(args, double);
		ret = gen_uniform_continuous(res, n, a, b);
		break;
	}
	case BINO_DISTRIBUTION: {
		// TODO
		break;
	}
	case NORMAL_DISTRIBUTION: {
		double mu = va_arg(args, double);
		double sigma = va_arg(args, double);
		ret = gen_normal(res, n, mu, sigma);
		break;
	}
	case EXP_DISTRIBUTION: {
		double lambda = va_arg(args, double);
		ret = gen_exponential(res, n, lambda);
		break;
	}
	case POSSION_DISTRIBUTION: {
		double lambda = va_arg(args, double);
		ret = gen_possion(res, n, lambda);
		break;
	}
	default:
		ret = -1;
		break;
	}
	va_end(args);
	return ret;
}
