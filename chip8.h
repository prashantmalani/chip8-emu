/*
 * chip8.h
 *
 * Contains macro definitions used by chip8.c
 * Also contains declaration of functions which are called outside of chip8.c
 *
 * Author: Prashant Malani <p.malani@gmail.com>
 * Date  : 03/21/2015
 *
 */
#ifndef _CHIP8_H_
#define _CHIP8_H_

#include "debug.h"

/* Common address locations */
#define FONTSET_BEGIN		0x50
#define PROGRAM_BEGIN		0x200
#define MAX_MEM			0x1000
#define SCREEN_X		64
#define SCREEN_Y		32

#define SDL			1

#define GFX_SCALE		16

#define SLEEP_CYCLE_DURATION	16

int initialize(void);
void *execute(void *);
int loadProgram(char *);

#endif
