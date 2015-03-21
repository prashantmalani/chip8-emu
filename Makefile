LFLAGS = `sdl-config --libs` -lSDL -pthread
default: chip8

chip8.o: chip8.c
	gcc -c -g chip8.c -o chip8.o $(LFLAGS)

main.o: main.c
	gcc -c -g main.c -o main.o $(LFLAGS)

main: main.o chip8.o
	gcc -g main.o chip8.o -o main $(LFLAGS)

clean:
	-rm -rf main
	-rm -rf chip8.o
	-rm -rf main.o
