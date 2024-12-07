CXXFLAGS = -std=c++11
LDLIBS  = $(shell pkg-config --libs ncurses) -lboost_regex

BINS = regex

.PHONY: all
all: $(BINS)

regex: regex.cpp

.PHONY: clean
clean:
	rm -f $(BINS)
