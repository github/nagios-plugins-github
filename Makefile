CFLAGS=-Wall -Wextra -pedantic -std=c99 -Werror -lcurl $(shell pkg-config --cflags json)
LDFLAGS=$(shell pkg-config --libs json)

OBJECTS= \
	src/check_graphite

all: $(OBJECTS)

$(OBJECTS):
	$(CC) -o $@ $@.c $(CFLAGS) $(LDFLAGS)

install:
	mkdir -p $(PREFIX)/bin
	install -c -m 755 src/check_graphite $(PREFIX)/bin/check_graphite

.PHONY: clean

clean:
		rm -f $(SONAME) $(OBJECTS)
