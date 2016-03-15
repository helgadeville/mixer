DEBUGOPTS=

all: build build/mixer after

build:
	mkdir -p build

build/mixer: build/mix
	g++ -o build/mix build/mix.o ${DEBUGOPTS}

build/mix: src/main.cpp
	g++ -c -std=c++11 -Wall -o build/mix.o src/main.cpp ${DEBUGOPTS}

after:

clean:
	rm -rf build
