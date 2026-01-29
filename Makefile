# --- Project info --- #

NAME    = errands
VERSION = 49.0

# --- Project directories --- #

SRC_DIR  = src
DATA_DIR = data

BUILD_DIR         = _build
FLATPAK_BUILD_DIR = _flatpak-build
FLATPAK_REPO_DIR  = _flatpak-repo

# --- Configuration options --- #

# Pass them to `make` command like this: `make OPTION=VALUE`.

# Development mode: TRUE, FALSE. Default: FALSE.
# This option will set development APP_ID and change runtime directories and app icons.
DEVEL ?= FALSE
ifeq ($(DEVEL),TRUE)
	APP_ID = io.github.mrvladus.List.Devel
else
	APP_ID = io.github.mrvladus.List
endif

# Debug mode: GDB, GF2, FALSE.
# Run errands in the debugger.
DEBUG ?= FALSE
ifeq ($(DEBUG),GDB)
    RUN_CMD = gdb -q -ex run $(BUILD_DIR)/$(NAME)
else ifeq ($(DEBUG),GF2)
    RUN_CMD = gf2 $(BUILD_DIR)/$(NAME)
else
	RUN_CMD = $(BUILD_DIR)/$(NAME)
endif

# Installation prefix directory. Default: /usr/local.
# For Flatpak builds, set `prefix=/app`.
prefix  ?= /usr/local
# Installation destination directory. Default: not set.
DESTDIR ?=

# --- Installation directories --- #

bindir          = $(prefix)/bin
datarootdir     = $(prefix)/share
localedir       = $(datarootdir)/locale
desktopdir      = $(datarootdir)/applications
dbusdir         = $(datarootdir)/dbus-1/services
icondir         = $(datarootdir)/icons/hicolor
scalableicondir = $(icondir)/scalable/apps
symbolicicondir = $(icondir)/symbolic/apps

# --- Resources --- #

BLPS   = $(wildcard $(SRC_DIR)/*.blp)
STYLES = $(wildcard $(DATA_DIR)/styles/*.css)
ICONS  = $(wildcard $(DATA_DIR)/icons/*.svg)

GRESOURCE_XML = $(DATA_DIR)/$(NAME).gresource.xml
RESOURCES_C   = $(BUILD_DIR)/resources.c
RESOURCES_O   = $(BUILD_DIR)/resources.o

# --- Sources --- #

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS)) $(RESOURCES_O)
DEPS = $(OBJS:.o=.d)

# --- Compilation variables --- #

PKG_CONFIG_LIBS = libadwaita-1 gtksourceview-5 libical libportal-gtk4 libcurl libsecret-1

LDFLAGS    ?=
ALL_LDFLAGS = $(LDFLAGS) `pkg-config --libs $(PKG_CONFIG_LIBS)`
CFLAGS     ?=
ALL_CFLAGS  = $(CFLAGS) \
			-Wall -g -std=c11 -D_GNU_SOURCE \
			`pkg-config --cflags $(PKG_CONFIG_LIBS)` \
			-DVERSION='"$(VERSION)"' \
			-DVERSION_COMMIT='"$(shell git rev-parse --short HEAD)"' \
			-DAPP_ID='"$(APP_ID)"' \
			-DLOCALE_DIR='"$(localedir)"'

# --- Targets --- #

all: $(BUILD_DIR)/$(NAME)

clean:
	@rm -rf $(BUILD_DIR) .flatpak*

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# --- Resources targets --- #

$(RESOURCES_O): $(RESOURCES_C) | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(ALL_CFLAGS) -MMD -MP -c -o $@ $<

$(RESOURCES_C): $(GRESOURCE_XML) $(BLPS) $(STYLES) $(ICONS)
	@echo "Embedding resources into $@"
	@blueprint-compiler batch-compile $(BUILD_DIR) $(SRC_DIR) $(BLPS)
	@glib-compile-resources --generate-source --target=$@ --c-name=$(NAME) $<

# --- Sources targets --- #

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(ALL_CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/$(NAME): $(OBJS)
		@echo "Linking executable $@"
		@$(CC) -o $@ $^ $(ALL_LDFLAGS)

-include $(DEPS)

# --- Installation targets --- #

install: $(BUILD_DIR)/$(NAME)
	# Executable
	install -Dsm 755 $(BUILD_DIR)/$(NAME) $(DESTDIR)$(bindir)/$(NAME)
	# Desktop file
	@cp $(DATA_DIR)/$(NAME).desktop.in $(BUILD_DIR)/$(APP_ID).desktop
	@sed -i "s/@APP_ID@/$(APP_ID)/g" $(BUILD_DIR)/$(APP_ID).desktop
	@sed -i "s|@BIN_DIR@|$(bindir)|g" $(BUILD_DIR)/$(APP_ID).desktop
	install -Dm 644 $(BUILD_DIR)/$(APP_ID).desktop $(DESTDIR)$(desktopdir)/$(APP_ID).desktop
	# Icons
	install -Dm 644 $(DATA_DIR)/icons/$(APP_ID).svg $(DESTDIR)$(scalableicondir)/$(APP_ID).svg
	install -Dm 644 $(DATA_DIR)/icons/io.github.mrvladus.List-symbolic.svg $(DESTDIR)$(symbolicicondir)/io.github.mrvladus.List-symbolic.svg
	# D-Bus service
	@cp $(DATA_DIR)/$(NAME).service.in $(BUILD_DIR)/$(APP_ID).service
	@sed -i "s/@APP_ID@/$(APP_ID)/g" $(BUILD_DIR)/$(APP_ID).service
	@sed -i "s|@BIN_DIR@|$(bindir)|g" $(BUILD_DIR)/$(APP_ID).service
	install -Dm 644 $(BUILD_DIR)/$(APP_ID).service $(DESTDIR)$(dbusdir)/$(APP_ID).service

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(NAME)
	rm -f $(DESTDIR)$(desktopdir)/$(APP_ID).desktop
	rm -f $(DESTDIR)$(scalableicondir)/$(APP_ID).svg
	rm -f $(DESTDIR)$(symbolicicondir)/io.github.mrvladus.List-symbolic.svg
	rm -f $(DESTDIR)$(dbusdir)/$(APP_ID).service

# --- Flatpak targets --- #

$(FLATPAK_BUILD_DIR): $(OBJS)
	flatpak-builder --ccache --force-clean --repo=$(FLATPAK_REPO_DIR) $@ tools/flatpak/$(APP_ID).json

flatpak-run: $(FLATPAK_BUILD_DIR)
	flatpak-builder --run $< tools/flatpak/$(APP_ID).json $(NAME)

flatpak-bundle: $(FLATPAK_BUILD_DIR) | $(BUILD_DIR)
	flatpak build-bundle $(FLATPAK_REPO_DIR) $(BUILD_DIR)/$(APP_ID).flatpak $(APP_ID) --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo

# --- Development targets --- #

# Setup distrobox environment.
distrobox-setup:
	bash tools/distrobox/setup.sh

# Build and run the application.
run: all
	$(RUN_CMD)

# Count source lines of code.
sloc:
	@find $(SRC_DIR) -name '*.c' -o -name '*.h' | sort | xargs wc -l

.PHONY: all install uninstall run clean sloc flatpak-run flatpak-bundle
