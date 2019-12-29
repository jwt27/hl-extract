CXX := g++
CXXFLAGS += -O3 -flto -std=gnu++17
CXXFLAGS += -Wall -Wextra

.PHONY: all clean

all: hl-extract hl-convert-snd hl-convert-ggs

hl-extract: hl-extract.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

hl-convert-snd: hl-convert-snd.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ -lFLAC++

hl-convert-ggs: hl-convert-ggs.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpng

clean:
	-rm -f hl-extract hl-convert-snd hl-convert-ggs
