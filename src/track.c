#include "playlist.h"
#include "u8string.h"

struct track {
	u8s title;
	u8s album;
	u8s *artist;
	char *urls[];
};
