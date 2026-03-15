CXX = g++
CFLAGS = -g -Wall -Wextra -Werror -std=c++17
EXPORT := -Wl,-E
SKYFALL_LIBS := -lpthread -ldl

SHARED = -fPIC --shared
SKYFALL_DIR = .
SKYFALL_SRC = ./src
SKYFALL_BUILD_PATH ?= .
SKYFALL_EXAMPLES = ./examples

all: $(SKYFALL_BUILD_PATH)/skyfall $(SKYFALL_EXAMPLES)/chat/testchat.so $(SKYFALL_EXAMPLES)/signal/testsignal.so

$(SKYFALL_BUILD_PATH)/skyfall: $(SKYFALL_SRC)/kernel/*.cc $(SKYFALL_SRC)/util/*.cc
	$(CXX) $(CFLAGS) -o $@ $^ -I$(SKYFALL_SRC) $(EXPORT) $(SKYFALL_LIBS)

examples/chat/testchat.so: $(SKYFALL_EXAMPLES)/chat/*.cc
	$(CXX) $(CFLAGS) $(SHARED) -I$(SKYFALL_EXAMPLES)/chat -I$(SKYFALL_SRC) $^ -o $@

examples/signal/testsignal.so: $(SKYFALL_EXAMPLES)/signal/*.cc
	$(CXX) $(CFLAGS) $(SHARED) -I$(SKYFALL_EXAMPLES)/signal -I$(SKYFALL_SRC) $^ -o $@

check:
	cppcheck --enable=all --std=c++17 --suppress=missingIncludeSystem -I $(SKYFALL_SRC)/kernel -I $(SKYFALL_SRC)/util $(SKYFALL_SRC)/kernel 2>&1
	cppcheck --enable=all --std=c++17 --suppress=missingIncludeSystem -I $(SKYFALL_SRC)/util $(SKYFALL_SRC)/util 2>&1
	cppcheck --enable=all --std=c++17 --suppress=missingIncludeSystem -I $(SKYFALL_SRC)/kernel -I $(SKYFALL_SRC)/util $(SKYFALL_EXAMPLES)/chat 2>&1
	cppcheck --enable=all --std=c++17 --suppress=missingIncludeSystem -I $(SKYFALL_SRC)/kernel -I $(SKYFALL_SRC)/util $(SKYFALL_EXAMPLES)/signal 2>&1

clean:
	rm -f $(SKYFALL_BUILD_PATH)/skyfall
	rm -f $(SKYFALL_EXAMPLES)/chat/testchat.so $(SKYFALL_EXAMPLES)/signal/testsignal.so