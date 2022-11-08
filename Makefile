CFLAGS := -fanalyzer -Wall -Wextra -Werror -Os -std=c11 -Iinclude

all: dodo 
.PHONY: all clean

%: src/%.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf dodo
