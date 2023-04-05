APP_ID=io.github.mrvladus.List
BIN=list
PREFIX=/app
CC=gcc

build:
	$(CC) -Wall `pkg-config --cflags --libs libadwaita-1` src/main.c -o $(BIN)

install: build
	install -D list /app/bin/list
	install -D data/$(APP_ID).desktop $(PREFIX)/share/applications/$(APP_ID).desktop
	install -D data/icons/$(APP_ID).svg $(PREFIX)/share/icons/hicolor/scalable/apps/$(APP_ID).svg
	install -D data/icons/$(APP_ID)-symbolic.svg $(PREFIX)/share/icons/hicolor/symbolic/apps/$(APP_ID)-symbolic.svg
	install -D data/$(APP_ID).metainfo.xml $(PREFIX)/share/metainfo/$(APP_ID).metainfo.xml
	install -D data/$(APP_ID).gschema.xml $(PREFIX)/share/glib-2.0/schemas/$(APP_ID).gschema.xml
	glib-compile-schemas $(PREFIX)/share/glib-2.0/schemas

run:
	flatpak-builder --user --install --force-clean _build $(APP_ID).yaml
	flatpak run $(APP_ID)
