#include "progress.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

int main()
{
	progress *p    = progress_new(false);
	const char *names[] = {"test"};
	prog_bar *b    = progress_bar_add(p, names[0], 20);

	size_t processed = 0;
	size_t estimated = 20;
	size_t seed      = 42;

	std::cout << "Simulating diffusion via progress.cpp API:\n\n";

	while (estimated > 0) {
		progress_bar_update(b, processed, estimated, "diffusion");
		std::this_thread::sleep_for(std::chrono::milliseconds(80));

		processed += 1;
		estimated -= 1;

		seed = seed * 1103515245 + 12345;
		if (seed % 3 == 0 && estimated < 30) {
			size_t added = seed % 5 + 1;
			estimated += added;
		}
	}

	progress_bar_update(b, processed, processed, "diffusion");

	std::cout << "\n\nDone: processed=" << processed << "\n";
	progress_free(p);
	return 0;
}
