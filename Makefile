CC = gcc
CFLAGS = -Wall -Wextra -O2

GTK_CFLAGS = $(shell pkg-config --cflags libadwaita-1)
GTK_LIBS = $(shell pkg-config --libs libadwaita-1)
SSL_CFLAGS = $(shell pkg-config --cflags openssl)
SSL_LIBS = $(shell pkg-config --libs openssl)

CLI_SRC = metroapp/main.c metroapp/pathfinder.c
GTK_SRC = gtkapp/main.c gtkapp/app-window.c gtkapp/metro-map.c \
          gtkapp/station-coords.c gtkapp/route-panel.c metroapp/pathfinder.c

.PHONY: all cli gtk clean

all: cli gtk

cli: $(CLI_SRC)
	$(CC) $(CFLAGS) -o metro-cli $^ $(SSL_CFLAGS) $(SSL_LIBS)

gtk: $(GTK_SRC)
	$(CC) $(CFLAGS) -o metro-gtk $^ $(GTK_CFLAGS) $(GTK_LIBS) $(SSL_CFLAGS) $(SSL_LIBS)

clean:
	rm -f metro-cli metro-gtk
