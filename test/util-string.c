#include "utils/str.h"
#include "xmalloc.h"
#include <stdio.h>

int main()
{
	const char *path = "/player/{test}/station/{username}/{tb}recommanded{hal}";

	char *res = parseKVFormat(path,
			MAKE_KV("username", "tofweod"),
			MAKE_KV("test", "tta"),
			MAKE_KV("hal", "pp"), KV_END);

	printf("%s\n", res);
	xfree(res);

	return 0;
}
