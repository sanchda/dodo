INCLUDES = -Iinclude

all: find
.PHONY: all clean

%: src/%.c
	$(CC) -ggdb3 -m64 -std=c99 $(INCLUDES) -o $@ $< -lpthread

clean:
	rm -rf dodo
