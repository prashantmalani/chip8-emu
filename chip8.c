/*
 * chip8.c
 *
 * Contains emulator state, including registers, timer and graphics buffer,
 * along with opcode interpreter.
 *
 * Author: Prashant Malani <p.malani@gmail.com>
 * Date:   03/21/2015
 *
 */
#include "chip8.h"
#include <fcntl.h>
#include <stdbool.h>
#include "SDL/SDL.h"

#define STACK_SIZE	16
bool draw;
bool quitProgram = false;

/* Chip 8 hardware registers and machine state */
uint16_t opcode;
uint8_t mem[MAX_MEM];

uint8_t V[16];
uint16_t I;
uint16_t pc;

/* Graphics buffer holding screen state */
uint32_t gfx[SCREEN_X * SCREEN_Y];

uint8_t delay_timer;
uint8_t sound_timer;

uint16_t stack[STACK_SIZE];
/* Stack pointer points to top-most used index of stack[] array */
uint16_t sp;

/*
 * Structure to hold key press information.
 *
 * The key mapping is as follows:
 *
 *      Chip 8         --->        Keyboard
 *     1--2--3--C                 1--2--3--4
 *     4--5--6--D                 Q--W--E--R
 *     7--8--9--E                 A--S--D--F
 *     A--0--B--F                 Z--X--C--V
 */
uint8_t key[16];

SDL_Event kbEvent;

/* SDL Surface */
SDL_Surface *screen;

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

/*
 * Function: initialize
 * ---------------------
 *
 * Initialize the emulator hardware state and start up the SDL Surface.
 *
 * Args: NONE
 *
 * Returns:
 *	0 on success.
 *	Non-zero value on error.
 */
int initialize()
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

	sp = -1;

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
	return 0;
}

/*
 * Function: loadProgram
 * ----------------------
 *
 * Loads a given program binary into the emulator memory.
 *
 * Args:
 *	filepath: String pointer pointing to the filepath name.
 *
 * Returns:
 *	0 on success
 *	Non-zero value on error.
 */
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

/*
 * Function: updateTimers
 * ----------------------
 *
 * Update the values of the delay and sound timer.
 * Called once every cycle.
 *
 * Args: NONE
 *
 * Returns: NONE
 */
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

/*
 * Function: dumpScreen
 * --------------------
 *
 * Dump the contents of the graphics buffer onto STDOUT
 * Used for debug purposes.
 *
 * Args: NONE
 *
 * Returns: NONE
 */
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

/*
 * Function: handle8case
 * ---------------------
 *
 * Sub-routine to handle execution of all opcodes that have a prefix of 0x8000.
 * Should only be called from execute().
 *
 * Args:
 *	opcode: The opcode being executed this cycle.
 *
 * Returns: NONE
 */
void handle8case(uint16_t opcode)
{
	switch(opcode & 0xF) {
	case 0x0:
		// Case 8XY0: Sets VX to the value of VY.
		V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x1:
		// Case 8XY1: Sets VX to VX or VY.
		V[(opcode & 0xF00) >> 8] |= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x2:
		// Case 8XY2: Sets VX to VX and VY.
		V[(opcode & 0xF00) >> 8] &= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x3:
		// Case 8XY3: Sets VX to VX xor VY.
		V[(opcode & 0xF00) >> 8] ^= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x4:
		// Case 8XY4: Adds VY to VX. VF is set to 1 when there's a
		// carry, and to 0 when there isn't.
		if (V[(opcode & 0xF00) >> 8] + V[(opcode & 0xF0) >> 4] >
		    0xFFFF)
			V[0xF] = 1;
		else
			V[0xF] = 0;
		V[(opcode & 0xF00) >> 8] += V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x5:
		// Case 8XY5: Subtracts VY from VX. VF is set to 1 when there's
		// a borrow, and to 0 when there isn't.
		if (V[(opcode & 0xF00) >> 8] < V[(opcode & 0xF0) >> 4])
			V[0xF] = 1;
		else
			V[0xF] = 0;
		V[(opcode & 0xF00) >> 8] -= V[(opcode & 0xF0) >> 4];
		pc += 2;
		break;

	case 0x6:
		// Case 8XY6:Shifts VX right by one. VF is set to the value of
		// the least significant bit of VX before the shift.
		V[0xF] = V[(opcode & 0xF00) >> 8] & 1;
		V[(opcode & 0xF00) >> 8] >>= 1;
		pc += 2;
		break;

	case 0x7:
		// Case 8XY7: Sets VX to VY minus VX. VF is set to 1 when there's
		// a borrow, and to 0 when there isn't.
		if (V[(opcode & 0xF00) >> 8] > V[(opcode & 0xF0) >> 4])
			V[0xF] = 1;
		else
			V[0xF] = 0;
		V[(opcode & 0xF00) >> 8] = V[(opcode & 0xF0) >> 4] -
			V[(opcode & 0xF00) >> 8];
		pc += 2;
		break;

	case 0xE:
		// Case 8XYE: Shifts VX right by one. VF is set to the value of
		// the least significant bit of VX before the shift.
		V[0xF] = (V[(opcode & 0xF00) >> 8] & 0x8000) >> 15;
		V[(opcode & 0xF00) >> 8] <<= 1;
		pc += 2;
		break;

	default:
		LOGE("Unknown opcode %02x\n", opcode);
		pc += 2;
		break;
	}
}


/*
 * Function: handleFcase
 * ---------------------
 *
 * Sub-routine to handle execution of all opcodes that have a prefix of 0xF000.
 * Should only be called from execute()
 *
 * Args:
 *	opcode: The opcode being executed this cycle.
 *
 * Returns: NONE
 */
void handleFcase(uint16_t opcode)
{
	switch(opcode & 0xFF) {
	case 0x07:
		// Case FX07: Sets VX to the value of the delay timer.
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

/*
 * Function: handle0case
 * ---------------------
 *
 * Sub-routine to handle execution of all opcodes that have a prefix of 0x0000.
 * Should only be called from execute()
 *
 * Args:
 *	opcode: The opcode being executed this cycle.
 *
 * Returns: NONE
 */
void handle0case(uint16_t opcode)
{
	unsigned i,j;

	switch (opcode) {
	case 0x00E0:
		// Case: Clears the screen.
		for (i = 0; i < SCREEN_X; i++)
			for (j = 0; j < SCREEN_Y; j++)
				gfx[(i * SCREEN_X) + j] = 0;
		pc += 2;
		draw = true;
		break;

	case 0x00EE:
		// Case: Return from a sub-routine.
		if (sp < 0) {
			LOGE("No stack address to return to!\n");
			quitProgram = true;
			return ;
		}
		// Pop address off the stack, and decrement stack pointer.
		pc = stack[sp--];
		break;

	default:
		// TODO: Case 0NNN: Call RCA 1802 program at address NNN.
		break;
	}
}

/*
 * Function: handleOpcode
 * ----------------------
 *
 * Handle the opcode and act accordingly.
 * Called once every cycle.
 * NOTE: It is assumed that the pc modifications will occur here itself.
 *
 * Args:
 *	opcode: The opcode currently being executed.
 *
 * Returns: NONE
 */
void handleOpcode(uint16_t opcode)
{
	unsigned x, y;
	unsigned i, j, n;
	uint8_t cur_pixel;
	uint16_t tmp;
	LOGD("Fetched opcode is %02x\n", opcode);
	switch(opcode & 0xF000) {
	case 0x0000:
		handle0case(opcode);
		break;

	case 0x1000:
		// Case 1NNN: Jump to address at NNN
		pc = opcode & 0xFFF;
		break;

	case 0x2000:
		// Case 2NNN: Call subroutine at NNN
		// NOTE: Have to be careful here, and store the next address
		// in the PC. If you store the current PC, then when you finally
		// return from the subroutine, you'll execute this current PC
		// again, and get into a loop.
		//
		// At least that's what I *think* will happen....
		if (sp == STACK_SIZE - 1) {
			LOGE("Ran out of stack space!!\n");
			quitProgram = true;
			break;
		}
		stack[++sp] = (pc + 2);
		pc = opcode & 0xFFF;
		break;

	case 0x3000:
		// Case: Skips the next instruction if VX equals NN.
		if (V[(opcode & 0xF00) >> 8] == (opcode & 0xFF))
			pc +=2;
		pc +=2;
		break;

	case 0x4000:
		// Case: Skips the next instruction if VX doesn't equal NN.
		if (V[(opcode & 0xF00) >> 8] != (opcode & 0xFF))
			pc +=2;
		pc +=2;
		break;

	case 0x5000:
		// Case: Skips the next instruction if VX equals VY.
		if (V[(opcode & 0xF00) >> 8] == V[(opcode & 0xF0) >> 4])
			pc +=2;
		pc +=2;
		break;

	case 0x6000:
		// Case 6XNN: Sets VX to NN.
		V[(opcode & 0xF00) >> 8] = opcode & 0xFF;
		pc += 2;
		break;

	case 0x7000:
		// Case 7XNN: Add NN to VX
		tmp = V[(opcode & 0xF00) >> 8];
		V[(opcode & 0xF00) >> 8] += (opcode & 0xFF);
		// TODO: Find whether Carry flag needs to be set. Spec says no.
		// if (V[(opcode & 0xF00) >> 8] < tmp)
		//	V[0xF] = 1;
		pc += 2;
		break;

	case 0x8000:
		handle8case(opcode);
		break;

	case 0x9000:
		// Case: Skips the next instruction if VX equals VY.
		if (V[(opcode & 0xF00) >> 8] != V[(opcode & 0xF0) >> 4])
			pc +=2;
		pc +=2;
		break;

	case 0xA000:
		// Case ANNN: Set I to address NNN.
		I = opcode & 0xFFF;
		pc += 2;
		break;

	case 0xB000:
		// Case BNNN: Jumps to the address NNN plus V0.
		pc = (opcode & 0xFFF) + V[0];
		break;

	case 0xC000:
		// Case CXNN: Sets VX to a random number, masked by NN.
		V[(opcode & 0xF00) >> 8] = (rand() % 0xFF) & (opcode & 0xFF);
		pc += 2;
		break;

	case 0xD000:
		// Case DXYN: Draw the sprite at coordinate VX, VY, height N px
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
					gfx[((y + j) * SCREEN_X) + x + i] ^=
						0xFFFFFFFF;
				}
			}
		}

		pc += 2;
		break;

	case 0xF000:
		handleFcase(opcode);
		break;

	default:
		LOGE("Unknown opcde %02x\n", opcode);
		pc += 2;
		break;
	}
}

/*
 * Function: updateKeyStruct
 * -------------------------
 *
 * Called from the SDL KeyEvent handler.
 * Updates the keypad structure to reflect which key has been pressed/released.
 *
 * Args:
 *	event: The SDL_Event being executed
 *	keyDown: Whether the key was pressed down.
 *
 * Returns: NONE
 */
void updateKeyStruct(SDL_Event event, bool keyDown)
{
	uint8_t key_index;

	switch (event.key.keysym.sym) {
	case SDLK_1:
		LOGD("1 Pressed\n");
		key_index = 0x1;
		break;
	case SDLK_2:
		LOGD("2 Pressed\n");
		key_index = 0x2;
		break;
	case SDLK_3:
		LOGD("3 Pressed\n");
		key_index = 0x3;
		break;
	case SDLK_4:
		LOGD("C Pressed\n");
		key_index = 0xC;
		break;
	case SDLK_q:
		LOGD("4 Pressed\n");
		key_index = 0x4;
		break;
	case SDLK_w:
		LOGD("5 Pressed\n");
		key_index = 0x5;
		break;
	case SDLK_e:
		LOGD("6 Pressed\n");
		key_index = 0x6;
		break;
	case SDLK_r:
		LOGD("D Pressed\n");
		key_index = 0xD;
		break;
	case SDLK_a:
		LOGD("7 Pressed\n");
		key_index = 0x7;
		break;
	case SDLK_s:
		LOGD("8 Pressed\n");
		key_index = 0x8;
		break;
	case SDLK_d:
		LOGD("9 Pressed\n");
		key_index = 0x9;
		break;
	case SDLK_f:
		LOGD("E Pressed\n");
		key_index = 0xE;
		break;
	case SDLK_z:
		LOGD("A Pressed\n");
		key_index = 0xA;
		break;
	case SDLK_x:
		LOGD("0 Pressed\n");
		key_index = 0x0;
		break;
	case SDLK_c:
		LOGD("B Pressed\n");
		key_index = 0xB;
		break;
	case SDLK_v:
		LOGD("F Pressed\n");
		key_index = 0xF;
		break;
	default:
		LOGD("Invalid Key pressed\n");
		return;
	}
	key[key_index] = keyDown;
}


/*
 * Function: kbHandler
 * -------------------
 *
 * Handler of all Events occuring inside the SDL Window.
 * Should be executed once every cycle.
 *
 * Args: NONE
 *
 * Returns: NONE
 */
void kbHandler()
{
	if (SDL_PollEvent(&kbEvent)) {
		switch (kbEvent.type) {
		case SDL_QUIT:
			quitProgram = true;
			return;
		case SDL_KEYDOWN:
			updateKeyStruct(kbEvent, true);
		case SDL_KEYUP:
			updateKeyStruct(kbEvent, false);
			break;
		default:
			LOGD("Unknown key event detected %u\n", (uint32_t)kbEvent.type);
			break;
		}
	}
}

/* Function: drawScreen
 * --------------------
 *
 * Routine to draw the contents of the graphics buffer onto the SDL Window.
 *
 * Args:
 *	screen: Pointer to the SDL_Surface on which to draw.
 *
 * Returns: NONE
 */
void drawScreen(SDL_Surface *screen)
{
	int i;
	uint32_t *ptr;
	uint32_t *gfp = gfx, r, c, ci;
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

/*
 * Function: execute
 * -----------------
 *
 * Primary routine which fetches the opcode, handles it, performs book-keeping
 * of state and draws and updates the window if required.
 * Should execute once every 16ms to mimic a 60Hz operating frequency.
 *
 * Args:
 *	data: Pointer to optional data passed to the thread
 *
 * Returns: NONE
 */
void *execute(void *data)
{
	while (!quitProgram) {
		draw = false;
		// Fetch opcode
		opcode = mem[pc] << 8 | mem[pc + 1];
		handleOpcode(opcode);
		updateTimers();

		if (draw) {
			dumpScreen();
			drawScreen(screen);
		}
		kbHandler();

		// TODO: Maybe think about spawning this off in another thread;
		usleep(16 * 1000);
	}
	return;
}

