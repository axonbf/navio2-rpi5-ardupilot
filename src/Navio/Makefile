CXX ?= g++
CFLAGS = -std=c++11 -Wno-psabi -c -I .

SRC=$(wildcard Common/*.cpp Navio2/*.cpp)
OBJECTS = $(SRC:.cpp=.o) 

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ $< 

.PHONY: get_deps clean

all: get_deps $(OBJECTS)
	ar rcs libnavio.a $(OBJECTS)

get_deps:
	bash get_dependencies.sh

clean:
	rm -f Common/*.o Navio2/*.o *.a
