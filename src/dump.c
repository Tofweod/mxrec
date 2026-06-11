#include "dump.h"

static void dump2json(dumpHandle *dh, FILE *fp, void *ptr, size_t size)
{
	if (size <= 0)
		return;
	else if (size == 1)
		dh->dump2json(fp, ptr);
	else
		dh->listdump2json(fp, size, ptr);
}

void dump(dumpHandle *dh, FILE *fp, void *ptr, size_t size, enum dumpType type)
{
	switch (type) {
	case DUMP2JSON: {
		dump2json(dh, fp, ptr, size);
		break;
	}
	}
}
