APP_ID=io.github.mrvladus.List
PREFIX=/app

install:
	install -D src/list $(PREFIX)/bin/list
	install -D src/main.py $(PREFIX)/share/list/list/main.py
	install -D src/globals.py $(PREFIX)/share/list/list/globals.py
	install -D src/widgets/main_window.py $(PREFIX)/share/list/list/widgets/main_window.py
	install -D src/widgets/about_window.py $(PREFIX)/share/list/list/widgets/about_window.py
	install -D src/widgets/headerbar.py $(PREFIX)/share/list/list/widgets/headerbar.py
	install -D src/widgets/entry.py $(PREFIX)/share/list/list/widgets/entry.py
	install -D src/widgets/todolist.py $(PREFIX)/share/list/list/widgets/todolist.py
	install -D src/widgets/todo.py $(PREFIX)/share/list/list/widgets/todo.py

	install -D data/$(APP_ID).desktop $(PREFIX)/share/applications/$(APP_ID).desktop
	install -D data/icons/$(APP_ID).svg $(PREFIX)/share/icons/hicolor/scalable/apps/$(APP_ID).svg
	install -D data/icons/$(APP_ID)-symbolic.svg $(PREFIX)/share/icons/hicolor/symbolic/apps/$(APP_ID)-symbolic.svg
	install -D data/$(APP_ID).metainfo.xml $(PREFIX)/share/metainfo/$(APP_ID).metainfo.xml
	install -D data/$(APP_ID).gschema.xml $(PREFIX)/share/glib-2.0/schemas/$(APP_ID).gschema.xml
	glib-compile-schemas $(PREFIX)/share/glib-2.0/schemas

validate:
	flatpak run org.freedesktop.appstream-glib validate data/$(APP_ID).metainfo.xml

run:
	flatpak run org.flatpak.Builder --user --install --force-clean _build $(APP_ID).yaml
	flatpak run $(APP_ID)

clean:
	rm -rf ~/.var/app/$(APP_ID)
