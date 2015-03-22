
#include "debug.h"

/* Common address locations */
#define FONTSET_BEGIN		0x50
#define PROGRAM_BEGIN		0x200
#define MAX_MEM			0x1000
#define SCREEN_X		64
#define SCREEN_Y		32

#define SDL		1

#define GFX_SCALE	32

int initialize(void);
void *execute(void *);
int loadProgram(char *);
