CFLAGS := -Wall -Os -m64 -std=c99 -Iinclude

all: dodo 
.PHONY: all clean

%: src/%.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf dodo
