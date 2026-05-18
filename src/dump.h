#ifndef TOF_MXREC_DUMP_H
#define TOF_MXREC_DUMP_H

typedef struct dumpType {
	void (*dump2json)(void *ptr);
	void (*listdump2json)(void *ptr);
} dumpType;

#endif
