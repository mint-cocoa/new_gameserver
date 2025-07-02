CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
TARGET = test_client
SOURCES = test_client.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean 