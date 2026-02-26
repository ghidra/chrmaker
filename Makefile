CC     = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LIBS   = $(shell sdl2-config --libs)
SRC    = main.c chr.c render.c input.c export.c font.c
HDR    = chr.h main.h render.h input.h export.h panel.h font.h

chrmaker: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LIBS)

clean:
	rm -f chrmaker

.PHONY: clean
