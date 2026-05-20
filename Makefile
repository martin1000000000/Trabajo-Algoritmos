CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pedantic
ifeq ($(OS),Windows_NT)
TARGET := main.exe
else
TARGET := main
endif

SOURCES := main.cpp
HEADERS := include/data.hpp include/explicit_array.hpp include/gap_coding.hpp include/elias.hpp

.PHONY: all clean run-benchmark

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run-benchmark: $(TARGET)
	./$(TARGET) --benchmark

clean:
	rm -f main main.exe main_debug.exe benchmark_results.csv
