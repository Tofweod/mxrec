#ifndef TOF_MXERC_JSON_H
#define TOF_MXERC_JSON_H

#define MAX_JSON_ERR_LEN 1024

struct json_err {
	char msg[MAX_JSON_ERR_LEN];
	int length;
};

void write_jsonerr(void *err, const char *fmt,...);

void print_jsonerr(void *err);

#endif
