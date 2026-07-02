CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread $(shell pkg-config --cflags gtkmm-3.0 2>/dev/null)
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lvlc $(shell pkg-config --libs gtkmm-3.0 2>/dev/null)

SRC_DIR = src
BIN_DIR = bin

# Find all .cpp files in src/
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)

# Generate target executable names in bin/ corresponding to src/*.cpp
EXECUTABLES = $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%, $(SOURCES))

.PHONY: all clean

all: $(EXECUTABLES)

# Rule to build each executable from its corresponding .cpp file
$(BIN_DIR)/%: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	@echo "Compiling $< -> $@"
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# Create bin/ directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean built binaries
clean:
	@echo "Cleaning up $(BIN_DIR)..."
	rm -f $(BIN_DIR)/*
