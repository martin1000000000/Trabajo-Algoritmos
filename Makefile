CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pedantic
ifeq ($(OS),Windows_NT)
TARGET := main.exe
else
TARGET := main
endif

SOURCES := main.cpp
HEADERS := data.hpp explicit_array.hpp gap_coding.hpp elias.hpp

.PHONY: all clean run-benchmark

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run-benchmark: $(TARGET)
	./$(TARGET) --benchmark

clean:
	rm -f main main.exe main_debug.exe benchmark_results.csv
