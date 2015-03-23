/*
 * Main execution file for Chip 8 emulator.
 *
 *
 * Author: Prashant Malani <p.malani@gmail.com>
 * Date:   03/14/2015
 *
 */
#include "debug.h"
#include <pthread.h>
#include "chip8.h"

int debug_level = LOG_INFO;

/*
 * Function: main
 * --------------
 *
 * Main entry point of execution for the code.
 */
int main(int argc, char **argv)
{
	pthread_t execute_thread;

	if (argc < 2) {
		LOGE("Program path not provided.\n");
		return -1;
	}

	if (argc > 2) {
		if (!strcmp(argv[2], "--debug"))
			debug_level = LOG_DEBUG;
		else if (!strcmp(argv[2], "--error"))
			debug_level = LOG_ERROR;
	}
	LOGI("Initializing hardware\n");

	if (initialize())
		return -1;

	if(loadProgram(argv[1]))
		return -1;

	pthread_create(&execute_thread, 0, execute, NULL);

	pthread_join(execute_thread, NULL);
	SDL_Quit();
	return 0;
}
