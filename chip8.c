/*
 * Main execution file for Chip 8 emulator
 *
 * Author: Prashant Malani <p.malani@gmail.com>
 * Date:   03/14/2015
 */
#include "debug.h"
#include <fcntl.h>

/* Common address locations */
#define FONTSET_BEGIN		0x50
#define PROGRAM_BEGIN		0x200

/* TODO: We should eventually shift this to the main function */
int debug_level = LOG_DEBUG;

/* Chip 8 hardware registers and machine state */
unsigned short opcode;
unsigned char mem[4096];

unsigned char V[16];
unsigned short I;
unsigned short pc;

unsigned char gfx[64 *32];

unsigned char delay_timer;
unsigned char sound_timer;

unsigned short stack[16];
/* Stack pointer points to the index of stack[] array */
unsigned short sp;
unsigned char key[16];

/* Chip 8 built in font set */
unsigned char font_set[80] =
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
	for (i = 0; i < (64 *32); i++)
		gfx[i] = 0;

	// Clear timers
	delay_timer = 0;
	sound_timer = 0;

	// Clear I and PC registers
	I = 0;
	pc = 0;

	/* Load fontset into memory */
	for (i = 0; i < 80; i++)
		mem[80 + i] = font_set[i];
}

int loadProgram(char *filepath)
{
	FILE *fp = NULL;
	fp = fopen(filepath, O_RDONLY);
	if (!fp) {
		LOGE("Error opening file %s\n", filepath);
		return -1;
	}
	fclose(fp);
	return 0;
}

int main()
{
	LOGI("Initializing hardware\n");
	initialize();
	return 0;
}
