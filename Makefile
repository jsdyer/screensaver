# Use the same Linux-to-Windows compiler as StarfieldSaver.
CROSS ?= x86_64-w64-mingw32-
CC = $(CROSS)gcc
WINDRES = $(CROSS)windres
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDLIBS = -lgdi32 -luser32 -ladvapi32 -lm

SCENES := aquarium space
SAVERS := $(addprefix build/,$(addsuffix .scr,$(SCENES)))
COMMON := src/saver.c src/canvas.c
HEADERS := src/scene.h src/canvas.h src/resource.h

.PHONY: all clean
all: $(SAVERS)

# Each scene supplies its Windows display name and shares the settings dialog.
build/%.res.o: src/%.rc src/settings.rcinc src/resource.h | build
	$(WINDRES) -I src -i $< -o $@

build/%.scr: src/%.c $(COMMON) $(HEADERS) build/%.res.o | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -mwindows $(LDFLAGS) -o $@ $(COMMON) $< build/$*.res.o $(LDLIBS)

build:
	mkdir -p $@

clean:
	rm -rf build
