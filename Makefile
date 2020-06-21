CC=gcc
CFLAGS=-Wall -O2 $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS=-lm -ldl $(shell pkg-config --libs gtk+-3.0)

SRC=main.c index.qjs.c # repl.qjs.c
OBJ=$(SRC:.c=.o) \

ifeq ($(RELEASE),)
	OBJ+=/usr/local/lib/quickjs/libquickjs.a
else
	CFLAGS+=-flto
	OBJ+=/usr/local/lib/quickjs/libquickjs.lto.a
endif

main: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# %.qjs.c: %.js
#	qjsc -c -o $@ -m $<
index.qjs.c: index.js repl.js # mithril.js
	qjsc -c -o $@ -m $^

.PHONY: clean
clean:
	rm -f main *.o *.qjs.c

.PHONY: test
test: main
	./main
