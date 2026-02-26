CXX = g++
CFLAGS = -g -Wall -std=c++17
EXPORT := -Wl,-E
SKYFALL_LIBS := -lpthread -ldl

SHARED = -fPIC --shared
SKYFALL_DIR = .
SKYFALL_SRC = ./src
SKYFALL_BUILD_PATH ?= .

all: $(SKYFALL_BUILD_PATH)/skyfall examples/chat/testchat.so

$(SKYFALL_BUILD_PATH)/skyfall: $(SKYFALL_SRC)/kernel/*.cc $(SKYFALL_SRC)/util/*.cc
	$(CXX) $(CFLAGS) -o $@ $^ -I$(SKYFALL_SRC) $(EXPORT) $(SKYFALL_LIBS)

examples/chat/testchat.so: examples/chat/*.cc
	$(CXX) $(CFLAGS) $(SHARED) -Iexamples/chat -I$(SKYFALL_SRC) $^ -o $@

clean:
	rm -f $(SKYFALL_BUILD_PATH)/skyfall
	rm -f examples/chat/testchat.so