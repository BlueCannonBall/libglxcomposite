CC = gcc
CFLAGS = -Wall -O2 -s -flto -ansi -shared -fPIC
TARGET = libcomposite.so

.PHONY: all clean install

all: $(TARGET) composite.rs

$(TARGET): composite.c composite.h
	$(CC) $< $(CFLAGS) -o $@

composite.rs: composite.h
	bindgen $< -o $@

clean:
	rm -f $(TARGET) composite.rs

install:
	cp $(TARGET) /usr/lib/