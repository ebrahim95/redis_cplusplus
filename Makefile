# Variables
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra
TARGET = redis
SOURCES = redis.cpp resp_deserializer.cpp resp_serializer.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Default target (runs when you type 'make')
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

# Compile .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rebuild everything from scratch
rebuild: clean all

# Mark these as not real files
.PHONY: all clean rebuild

