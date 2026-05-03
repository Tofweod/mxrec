#include "string.h"
#include <string.h>

// static auxiliary functions



// api

u8s u8snewlen(const void *init, u8s_size_t len) {

}

u8s u8snew(const char *init)
{
	u8s_size_t len = (init == NULL) ? 0 : strlen(init);
	return u8snewlen(init,len);

}
u8s u8snewplacement(char *buf, size_t bufsize, const char *init, u8s_size_t len);
