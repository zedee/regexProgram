PROGRAM = regex
CXXFLAGS = -std=c++11
LDLIBS = `pkg-config --libs-only-l ncurses` -lboost_regex -lboost_filesystem

.PHONY: all
.PHONY: clean

all: $(PROGRAM)

clean:
	rm -f $(PROGRAM)

$(PROGRAM): $(PROGRAM).cpp
	g++ $(PROGRAM).cpp $(CXXFLAGS) $(LDLIBS) -o $(PROGRAM)
