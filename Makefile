EXE=cf
CC=clang++
CFLAGS=-Wall --std=c++11 

.PHONY: all debug run clean

all: $(EXE)

$(EXE): main.o
	$(CC) -o $(EXE) main.o $(shell sdl2-config --libs) -lSDL2_image -lSDL2_mixer

main.o: main.cpp
	$(CC) -c $(CFLAGS) $(shell sdl2-config --cflags) main.cpp

run:
	./$(EXE)

clean: 
	rm -rf *.o $(EXE)
