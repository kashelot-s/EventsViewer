SOLUTION = eventsview

CC = gcc

CFLAGS += -m32
CFLAGS += -I./src
CFLAGS += `pkg-config --cflags gtk+-3.0`
LFLAGS += `pkg-config --libs gtk+-3.0`

ifdef DEBUG
CFLAGS += -O0 -ggdb
CFLAGS += -Wall -Wextra
endif

ifdef COVERAGE
CFLAGS += -g --coverage -fprofile-arcs -ftest-coverage
LFLAGS += -lgcov
endif

SOURCES = $(wildcard ./src/*.c)
OBJS = $(subst ./src,.,$(subst .c,.o,$(SOURCES)))

all: $(OBJS) $(SOURCES)
	$(CC) -o $(SOLUTION) $(OBJS) $(LFLAGS)

%.o: ./src/%.c
	$(CC) -c $(CFLAGS) $^

.PHONY:  clean
clean:
	-rm -f $(SOLUTION)
	-rm -rf *.o
	-rm -rf ./src/*.o
	-rm -rf *.gc*

