DEBUGOPTS=

all: build/oooshaker

build/oooshaker: build/oooshaker.o
	g++ -o build/oooshaker build/oooshaker.o ${DEBUGOPTS}

build/oooshaker.o: src/main.cpp
	g++ -c -std=c++11 -Wall -o build/oooshaker.o src/main.cpp ${DEBUGOPTS}
