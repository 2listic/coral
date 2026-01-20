# Makefile for entt_mwe

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
INCLUDES = -I./entt/src
TARGET = entt_mwe
SOURCE = entt_mwe.cpp

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)

# Run the executable
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Rebuild from scratch
rebuild: clean all

.PHONY: all run clean rebuild
