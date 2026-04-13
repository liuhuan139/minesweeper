CC = g++
CFLAGS = -Wall -O2 `pkg-config --cflags gtkmm-3.0`
LDFLAGS = `pkg-config --libs gtkmm-3.0`
TARGET = minesweeper
SOURCES = main.cc MineSweeper.cc
OBJECTS = $(SOURCES:.cc=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
