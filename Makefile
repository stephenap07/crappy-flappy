EXE=cf
CC=clang++
CFLAGS=-Wall --std=c++11 

.PHONY: all debug run clean

all: $(EXE)

$(EXE): obj/main.o
	$(CC) -o $(EXE) main.o $(shell sdl2-config --libs) -lSDL2_image -lSDL2_mixer

obj/main.o: src/main.cpp
	$(CC) -c -I include/ $(CFLAGS) $(shell sdl2-config --cflags) src/main.cpp

run:
	./$(EXE)

clean: 
	rm -rf *.o $(EXE)
