#ifndef TOF_MXERC_JSON_H
#define TOF_MXERC_JSON_H

#define MAX_JSON_ERR_LEN 1024

struct json_err {
	char msg[MAX_JSON_ERR_LEN];
	int length;
};

void write_jsonerr(void *err, const char *fmt, ...);

void print_jsonerr(void *err);

// helper macros
#define NARG_MAP(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define NARG_(...) NARG_MAP(__VA_ARGS__)
#define NARG(...) NARG_(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define YYJSON_GET2(root, k1) yyjson_obj_get((root), (k1))

#define YYJSON_GET3(root, k1, k2) yyjson_obj_get(YYJSON_GET2(root, k1), (k2))

#define YYJSON_GET4(root, k1, k2, k3) yyjson_obj_get(YYJSON_GET3(root, k1, k2), (k3))

#define YYJSON_GET5(root, k1, k2, k3, k4) yyjson_obj_get(YYJSON_GET4(root, k1, k2, k3), (k4))

#define YYJSON_GET6(root, k1, k2, k3, k4, k5) yyjson_obj_get(YYJSON_GET5(root, k1, k2, k3, k4), (k5))

#define YYJSON_GET7(root, k1, k2, k3, k4, k5, k6) yyjson_obj_get(YYJSON_GET6(root, k1, k2, k3, k4, k5), (k6))

#define YYJSON_GET8(root, k1, k2, k3, k4, k5, k6, k7) yyjson_obj_get(YYJSON_GET7(root, k1, k2, k3, k4, k5, k6), (k7))

#define YYJSON_GET9(root, k1, k2, k3, k4, k5, k6, k7, k8)                                                              \
	yyjson_obj_get(YYJSON_GET8(root, k1, k2, k3, k4, k5, k6, k7), (k8))

#define YYJSON_GET_DISP_(n, ...) YYJSON_GET##n(__VA_ARGS__)
#define YYJSON_GET_DISP(n, ...) YYJSON_GET_DISP_(n, __VA_ARGS__)
#define YYJSON_GET(...) YYJSON_GET_DISP(NARG(__VA_ARGS__), __VA_ARGS__)

#endif
