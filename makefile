CXX := g++
CXXFLAGS += -O3 -flto -std=gnu++17
CXXFLAGS += -Wall -Wextra

.PHONY: all clean

all: hl-extract hl-convert-audio

hl-extract: hl-extract.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

hl-convert-audio: hl-convert-audio.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ -lFLAC++

clean:
	-rm -f hl-extract hl-convert-audio
