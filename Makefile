CFLAGS=-Wall -Wextra -pedantic -std=c99 -Werror -lcurl $(shell pkg-config --cflags json-c)
LDFLAGS=$(shell pkg-config --libs json-c)

OBJECTS= \
	src/check_graphite

all: $(OBJECTS)

$(OBJECTS):
		$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $@.c

.PHONY: clean

clean:
		rm -f $(SONAME) $(OBJECTS)
