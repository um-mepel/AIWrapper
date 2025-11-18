# Compiler settings
CXX = g++
# C++ standard 17, enabling common warnings, and optimization level O2
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Linker flags for external libraries
# -lcurl is required for the cURL functionality
LDFLAGS = -lcurl

# Source file and target executable name
SRC = server_2.cpp
TARGET = server_2

.PHONY: all clean

# Default target
all: $(TARGET)

# Rule to compile the executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Rule to clean up compiled files
clean:
	@echo "--- Cleaning up compiled files ---"
	rm -f server
	rm -f server_2