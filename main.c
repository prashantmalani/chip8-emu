/*
 * Main execution file for Chip 8 emulator
 *
 * Author: Prashant Malani <p.malani@gmail.com>
 * Date:   03/14/2015
 */
#include "debug.h"
#include "SDL/SDL.h"
#include <pthread.h>
#include "chip8.h"

/* SDL Surface */
SDL_Surface *screen;

/*
 * TODO(pmalani) : Add Documentation
 */
int main(int argc, char **argv)
{
	pthread_t execute_thread;
	if (argc > 1) {
		if (!strcmp(argv[1], "--debug"))
			debug_level = LOG_DEBUG;
		else if (!strcmp(argv[1], "--error"))
			debug_level = LOG_ERROR;
	}
	LOGI("Initializing hardware\n");
	initialize();
	if(loadProgram("./zero_demo.ch8"))
		return -1;

#if SDL
	// Load SDL Window
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOGE("Couldn't initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	atexit(SDL_Quit);

	screen = SDL_SetVideoMode(SCREEN_X * GFX_SCALE, SCREEN_Y * GFX_SCALE,
			32, SDL_HWSURFACE | SDL_RESIZABLE);
	if (!screen) {
		LOGE("Couldn't  obtain a valid SDL surface: %s\n",
				SDL_GetError());
		return -1;
	}
#endif
	pthread_create(&execute_thread, 0, execute, screen);;

	pthread_join(execute_thread, NULL);
	SDL_Quit();
	return 0;
}
