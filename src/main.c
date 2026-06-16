#include "mxrec.h"
#include "monotonic.h"
#include "config.h"

config_t config;

int main(int argc, char **argv)
{
	globalCurlInit();

	globalCurlCleanup();
	return 0;
}
