/*
 * Main execution file for Chip 8 emulator
 *
 * Author: Prashant Malani <p.malani@gmail.com>
 * Date:   03/14/2015
 */
#include "debug.h"
#include <fcntl.h>
#include <stdbool.h>
#include "SDL/SDL.h"
#include <pthread.h>

/* Common address locations */
#define FONTSET_BEGIN		0x50
#define PROGRAM_BEGIN		0x200
#define MAX_MEM			0x1000
#define SCREEN_X		64
#define SCREEN_Y		32

#define SDL 1
#define GFX_SCALE	32

/* TODO: We should eventually shift this to the main function */
int debug_level = LOG_INFO;
bool draw;

/* Chip 8 hardware registers and machine state */
uint16_t opcode;
uint8_t mem[MAX_MEM];

uint8_t V[16];
uint16_t I;
uint16_t pc;

uint32_t gfx[SCREEN_X * SCREEN_Y];

uint8_t delay_timer;
uint8_t sound_timer;

uint16_t stack[16];
/* Stack pointer points to the index of stack[] array */
uint16_t sp;
uint8_t key[16];

/* SDL Surface */
SDL_Surface *screen;
SDL_Event kbEvent;

/* Chip 8 built in font set */
uint8_t font_set[80] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/* Reset all the memory and register values */
void initialize()
{
	int i;

	// Clear memory
	for (i = 0; i < 4096; i++)
		mem[i] = 0;

	// Clear Vx registers, keypad info and stack
	for (i = 0; i < 16; i++) {
		V[i] = 0;
		stack[i] = 0;
		key[i] = 0;
	}
	sp = 0;

	// Clear out graphics buffer
	for (i = 0; i < (SCREEN_X * SCREEN_Y); i++)
		gfx[i] = 0;

	// Clear timers
	delay_timer = 0;
	sound_timer = 0;

	// Initialize PC and I registers
	I = 0;
	pc = PROGRAM_BEGIN;

	/* Load fontset into memory */
	for (i = 0; i < 80; i++)
		mem[80 + i] = font_set[i];
}

int loadProgram(char *filepath)
{
	FILE *fp = NULL;
	long int fsize;
	int i;

	fp = fopen(filepath, "rb");
	if (!fp) {
		LOGE("Error opening file %s\n", filepath);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fsize > MAX_MEM - PROGRAM_BEGIN) {
		LOGE("Input file is larger than mem capacity\n");
		fclose(fp);
		return -1;
	}

	fread(&mem[PROGRAM_BEGIN], fsize, 1, fp);
	fclose(fp);

	LOGD("\n");
	// Print out the program contents for debug purposes
	for (i = 0; i < fsize; i++) {
		LOGD("%02x ", mem[PROGRAM_BEGIN + i]);
		if (!((i+1) % 8))
			LOGD("\n");
	}
	LOGD("\n");

	return 0;
}

void updateTimers()
{
		// Update timers
		if (delay_timer > 0)
			--delay_timer;

		if (sound_timer > 0) {
			if (sound_timer == 1)
				LOGD("BEEPER SOUNDED!");
			--sound_timer;
		}
}

void dumpScreen()
{
	int x;
		LOGD("\n");
		for (x = 0; x < (SCREEN_X * SCREEN_Y); x++) {
			// Convert it into binary form so it's easier to view
			LOGD("%c", gfx[x] ? 'W' : '-');
			if (!((x +1) % SCREEN_X))
				LOGD("\n");
		}
}

void handle8case(uint16_t opcode)
{
	switch(opcode & 0xF) {
	case 0x0:
		V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x1:
		V[(opcode & 0xF00) >> 8] |= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x2:
		V[(opcode & 0xF00) >> 8] &= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x3:
		V[(opcode & 0xF00) >> 8] ^= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x4:
		if (V[(opcode & 0xF00) >> 8] + V[(opcode & 0xF0) >> 4] >
		    0xFFFF)
			V[0xF] = 1;
		V[(opcode & 0xF00) >> 8] += V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	default:
		LOGE("Unknown opcode %02x\n", opcode);
		pc += 2;
		break;
	}
}

void handleFcase(uint16_t opcode)
{
	switch(opcode & 0xFF) {
	case 0x07:
		V[(opcode & 0xF00) >> 8] = delay_timer;
		pc +=2;
		break;

	case 0x0A:
		// TODO: Case 0xFX0A, a key press is awaited, and then stored in VX
		break;

	case 0x15:
		// Case FX15, set delay timer to VX
		delay_timer = V[(opcode & 0xF00) >> 8];
		pc += 2;
		break;

	case 0x18:
		// Case FX18, set sound timer to VX
		sound_timer = V[(opcode & 0xF00) >> 8];
		pc += 2;
		break;

	default:
		LOGE("Unknown opcode %02x\n", opcode);
		pc += 2;
		break;
	}
}

/* Handle the opcode and act accordingly.
 * It is assumed that the pc modifications will occur here itself
 */
void handleOpcode(uint16_t opcode)
{
	unsigned x, y;
	unsigned i, j, n;
	uint8_t cur_pixel;
	uint16_t tmp;
	LOGD("Fetched opcode is %02x\n", opcode);
	switch(opcode & 0xF000) {
	case 0x6000:
		// Case 6XNN: Sets VX to NN.
		V[(opcode & 0xF00) >> 8] = opcode & 0xFF;
		pc += 2;
		break;

	case 0xA000:
		// Case ANNN: Set I to address NNN.
		I = opcode & 0xFFF;
		pc += 2;
		break;

	case 0xD000:
		// Case DXYN: Draw the sprite at coordinate VX, VY, height N pixels
		draw = true;
		x = V[(opcode & 0xF00) >> 8];
		y = V[(opcode & 0xF0) >> 4];
		n = opcode & 0xF;
		for (j = 0; j < n; j++) {
			cur_pixel = mem[I + j];
			for (i = 0; i < 8; i++) {
				if (cur_pixel & (0x80 >> i)) {
					if(gfx[((y + j) * SCREEN_X) + x + i])
						V[0xF] = 1;
					gfx[((y + j) * SCREEN_X) + x + i] ^= 0xFFFFFFFF;
				}
			}
		}

		// TODO: Set a flag to draw something
		pc += 2;
		break;

	case 0x7000:
		// Case 7XNN: Add NN to VX
		tmp = V[(opcode & 0xF00) >> 8];
		V[(opcode & 0xF00) >> 8] += (opcode & 0xFF);
		if (V[(opcode & 0xF00) >> 8] < tmp)
			V[0xF] = 1;
		pc += 2;
		break;

	case 0xF000:
		handleFcase(opcode);
		break;

	case 0x4000:
		if (V[(opcode & 0xF00) >> 8] != (opcode & 0xFF))
			pc +=2;
		pc +=2;
		break;

	case 0x8000:
		handle8case(opcode);
		break;

	case 0x1000:
		pc = opcode & 0xFFF;
		break;

	default:
		LOGE("Unknown opcde %02x\n", opcode);
		pc += 2;
		break;
	}
}

void execute()
{
	int i;
	uint32_t *ptr;
	for (;;) {
		draw = false;
		// Fetch opcode
		opcode = mem[pc] << 8 | mem[pc + 1];

		handleOpcode(opcode);
		updateTimers();

		if (draw) {
			uint32_t *gfp = gfx, r, c, ci;
			dumpScreen();
#if SDL
			SDL_LockSurface(screen);
			ptr = (uint32_t *)screen->pixels;
			for (r = 0; r < SCREEN_Y * GFX_SCALE; r++) {
				for (c = 0; c < SCREEN_X; c++) {
					for (ci = 0; ci < GFX_SCALE; ci++)
						*ptr++ = *gfp;
					gfp++;
				}
				if ((r % GFX_SCALE) != GFX_SCALE - 1)
					gfp -= SCREEN_X;
			}
			SDL_UnlockSurface(screen);
			SDL_UpdateRect(screen, 0, 0, SCREEN_X * GFX_SCALE, SCREEN_Y * GFX_SCALE);
#endif
		}
		// TODO: Maybe think about spawning this off in another thread;
		usleep(16 * 1000);
	}
}

void *kbHandler(void *data)
{
	while (SDL_PollEvent(&kbEvent)) {
		switch (kbEvent.type) {
		case SDL_KEYDOWN:
			LOGI("Key press detected\n");
			break;
		case SDL_KEYUP:
			LOGI("Key release detected\n");
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	pthread_t kb_thread;
	if (argc > 1) {
		if (!strcmp(argv[1], "--debug"))
			debug_level = LOG_DEBUG;
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

	screen = SDL_SetVideoMode(SCREEN_X * GFX_SCALE, SCREEN_Y * GFX_SCALE, 32, SDL_HWSURFACE | SDL_RESIZABLE);
	if (!screen) {
		LOGE("Couldn't  obtain a valid SDL surface: %s\n", SDL_GetError());
		return -1;
	}
#endif
	pthread_create(&kb_thread, 0, kbHandler, NULL);
	execute();

	pthread_join(kb_thread, NULL);
	SDL_Quit();
	return 0;
}