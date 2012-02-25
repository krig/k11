CXX=clang++
CFLAGS=-g -std=c++0x
LDFLAGS=-g

.PHONY: clean

all: scheme

scheme: scheme.o
	$(CXX) $(LDFLAGS) -o $@ $^


%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf scheme *.o
