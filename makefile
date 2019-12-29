CXX := g++
CXXFLAGS += -O3 -flto -std=gnu++17
CXXFLAGS += -Wall -Wextra

.PHONY: all clean

all: hl-extract hl-convert-audio hl-convert-ggs

hl-extract: hl-extract.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

hl-convert-audio: hl-convert-audio.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ -lFLAC++

hl-convert-ggs: hl-convert-ggs.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	-rm -f hl-extract hl-convert-audio hl-convert-ggs
