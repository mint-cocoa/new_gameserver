CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
TARGET1 = test_client
TARGET2 = simple_client
SOURCES1 = test_client.cpp
SOURCES2 = simple_client.cpp

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(SOURCES1)
	$(CXX) $(CXXFLAGS) -o $(TARGET1) $(SOURCES1)

$(TARGET2): $(SOURCES2)
	$(CXX) $(CXXFLAGS) -o $(TARGET2) $(SOURCES2)

clean:
	rm -f $(TARGET1) $(TARGET2)

.PHONY: all clean 