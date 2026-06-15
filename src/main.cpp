#include <stdio.h>
#include <stdlib.h>

#include "gui.hh"

int main(int argc, char *argv[]) {
	chess::gui gui = chess::gui();

	gui.register_listen();
	gui.init();
	gui.run();

	return EXIT_SUCCESS;
}