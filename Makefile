CXX = g++
CXXFLAGS = -Wall -O3 -std=c++14 -shared -fPIC
TARGET = libglxcomposite.so
PREFIX = /usr/local

.PHONY: all clean install

all: $(TARGET) glxcomposite.rs

$(TARGET): glxcomposite.cpp glxcomposite.h
	$(CXX) $< $(CXXFLAGS) -o $@

glxcomposite.rs: glxcomposite.h
	bindgen $< -o $@

clean:
	rm -f $(TARGET) glxcomposite.rs

install:
	cp $(TARGET) $(PREFIX)/lib/
