#ifndef TOF_MXREC_SOURCE_H
#define TOF_MXREC_SOURCE_H

// function filed
typedef;

typedef struct source {
	/// security module
	/// some sources may restrict or even ban the access, 
	/// this module is used for bypass such limitations.
	void *security;

	void *userdata;
} source;

#endif // !TOF_MXREC_SOURCE_H
