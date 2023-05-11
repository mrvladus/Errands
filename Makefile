APP_ID=io.github.mrvladus.List
PREFIX=/app

install:
	# Source files
	install -D src/list $(PREFIX)/bin/list
	install -D src/main.py $(PREFIX)/share/list/list/main.py
	install -D src/task.py $(PREFIX)/share/list/list/task.py
	install -D src/sub_task.py $(PREFIX)/share/list/list/sub_task.py
	install -D src/utils.py $(PREFIX)/share/list/list/utils.py
	# GResource
	glib-compile-resources src/res/$(APP_ID).gresource.xml
	install -D src/res/$(APP_ID).gresource $(PREFIX)/share/list/$(APP_ID).gresource
	# GSchema
	install -D data/$(APP_ID).gschema.xml $(PREFIX)/share/glib-2.0/schemas/$(APP_ID).gschema.xml
	glib-compile-schemas $(PREFIX)/share/glib-2.0/schemas
	# Desktop file
	install -D data/$(APP_ID).desktop $(PREFIX)/share/applications/$(APP_ID).desktop
	# Icons
	install -D data/icons/$(APP_ID).svg $(PREFIX)/share/icons/hicolor/scalable/apps/$(APP_ID).svg
	install -D data/icons/$(APP_ID)-symbolic.svg $(PREFIX)/share/icons/hicolor/symbolic/apps/$(APP_ID)-symbolic.svg
	# Metadata
	install -D data/$(APP_ID).metainfo.xml $(PREFIX)/share/metainfo/$(APP_ID).metainfo.xml

validate:
	flatpak run org.freedesktop.appstream-glib validate data/$(APP_ID).metainfo.xml

run:
	flatpak run org.flatpak.Builder --user --install --force-clean _build $(APP_ID).yaml
	flatpak run $(APP_ID)

clean:
	rm -rf ~/.var/app/$(APP_ID)
