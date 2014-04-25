EXE=cf
CC=clang++
CFLAGS=-Wall --std=c++11 

.PHONY: all debug run clean

all: $(EXE)

debug: CFLAGS += -DDEBUG -g
debug: $(EXE)

$(EXE): obj/main.o
	$(CC) -o $(EXE) obj/main.o $(shell sdl2-config --libs) -lSDL2_image -lSDL2_mixer

obj/main.o: src/main.cpp
	$(CC) -o obj/main.o -c -I include/ $(CFLAGS) $(shell sdl2-config --cflags) src/main.cpp

run:
	./$(EXE)

clean: 
	rm -rf obj/*.o $(EXE)
