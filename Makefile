# Makefile for ta_tools_config project

CC= x86_64-w64-mingw32-gcc

CFLAGS=-O2 -Wpedantic -Wall -Wextra -ansi

bindir=$${TA_GAME_PATH}bin
bin_files=RemoteLogger.dll
csv=ta_tools_config.csv

objects=RemoteLogger.o ModLoader.o

all: RemoteLogger.dll

RemoteLogger.dll: $(objects)
	$(CC) $(CFLAGS) -o $@ -shared $(objects)

RemoteLogger.o: RemoteLogger.c RemoteLogger.h

ModLoader.o: ModLoader.c

clean:
	rm -f RemoteLogger.dll *.o

install:
	test -d original || mkdir original
	test -e "$(bindir)/RemoteLogger.dll" && \
	test ! -e original/RemoteLogger.dll && \
	cp -f "$(bindir)/RemoteLogger.dll" original/RemoteLogger.dll || true
	cp -f RemoteLogger.dll "$(bindir)"
	test -e $(csv) && cp -f $(csv) "$(bindir)" || true

uninstall:
	test -e original/RemoteLogger.dll && cp -f original/RemoteLogger.dll \
	"$(bindir)" || true
	rm -f "$(bindir)/$(csv)"

.PHONY: all clean install uninstall
