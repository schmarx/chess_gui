#include <stdio.h>
#include <stdlib.h>

#include "gui.hh"

int main(int argc, char *argv[]) {
	chess::gui gui = chess::gui();

	if (!gui.register_listen()) return EXIT_FAILURE;
	gui.init();
	gui.run();

	return EXIT_SUCCESS;
}