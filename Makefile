CC = gcc
CFLAGS = -Wall -O2 -flto -ansi -shared -fPIC
TARGET = libglxcomposite.so
PREFIX = /usr/local

.PHONY: all clean install

all: $(TARGET) glxcomposite.rs

$(TARGET): glxcomposite.c glxcomposite.h
	$(CC) $< $(CFLAGS) -o $@

glxcomposite.rs: glxcomposite.h
	bindgen $< -o $@

clean:
	rm -f $(TARGET) glxcomposite.rs

install:
	cp $(TARGET) $(PREFIX)/lib/
