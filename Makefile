default: chip8

chip8.0: chip8.c
	gcc -c -g chip8.c -o chip8.o

chip8: chip8.0
	gcc -g chip8.o -o chip8

clean:
	-rm -rf chip8
	-rm -rf chip8.o
