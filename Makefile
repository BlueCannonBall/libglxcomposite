CC = gcc
CFLAGS = -Wall -O2 -s -flto -ansi -shared -fPIC
TARGET = libcomposite.so

$(TARGET): composite.c composite.h
	$(CC) $^ $(CFLAGS) -o $@

.PHONY: clean install

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) /usr/lib/