# --- Project info --- #

NAME    = errands
APP_ID  = io.github.mrvladus.Errands
VERSION = 49.0

# --- Installation directories --- #

DESTDIR        ?=
prefix         ?= /usr/local
bindir          = $(prefix)/bin
datarootdir     = $(prefix)/share
localedir       = $(datarootdir)/locale
desktopdir      = $(datarootdir)/applications
dbusdir         = $(datarootdir)/dbus-1/services
icondir         = $(datarootdir)/icons/hicolor
scalableicondir = $(icondir)/scalable/apps
symbolicicondir = $(icondir)/symbolic/apps

# --- Project directories --- #

BUILD_DIR = build
SRC_DIR   = src
DATA_DIR  = data

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

CC = gcc
PKG_CONFIG_LIBS = libadwaita-1 gtksourceview-5 libical libportal libportal-gtk4 libcurl libsecret-1
CFLAGS    ?=
ALL_CFLAGS = $(CFLAGS) \
			-Wall -g -std=c11 -D_GNU_SOURCE \
			`pkg-config --cflags $(PKG_CONFIG_LIBS)` \
			-DVERSION='"$(VERSION)"' \
			-DVERSION_COMMIT='"$(shell git rev-parse --short HEAD)"' \
			-DAPP_ID='"$(APP_ID)"' \
			-DLOCALE_DIR='""'
LDFLAGS = `pkg-config --libs $(PKG_CONFIG_LIBS)`

# --- Targets --- #

all: $(BUILD_DIR)/$(NAME)

clean:
	@rm -rf $(BUILD_DIR)

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
		@$(CC) -o $@ $^ $(LDFLAGS)

-include $(DEPS)

# --- Installation targets --- #

install: $(BUILD_DIR)/$(NAME)
	# Executable
	install -Dsm 755 $(BUILD_DIR)/$(NAME) $(DESTDIR)$(bindir)/$(NAME)
	# Desktop file
	@cp $(DATA_DIR)/io.github.mrvladus.Errands.desktop.in $(BUILD_DIR)/$(APP_ID).desktop
	@sed -i "s/@APP_ID@/$(APP_ID)/g" $(BUILD_DIR)/$(APP_ID).desktop
	@sed -i "s|@BIN_DIR@|$(bindir)|g" $(BUILD_DIR)/$(APP_ID).desktop
	install -Dm 644 $(BUILD_DIR)/$(APP_ID).desktop $(DESTDIR)$(desktopdir)/$(APP_ID).desktop
	# Icons
	install -Dm 644 $(DATA_DIR)/icons/$(APP_ID).svg $(DESTDIR)$(scalableicondir)/$(APP_ID).svg
	install -Dm 644 $(DATA_DIR)/icons/io.github.mrvladus.Errands-symbolic.svg $(DESTDIR)$(symbolicicondir)/io.github.mrvladus.Errands-symbolic.svg
	# D-Bus service
	@cp $(DATA_DIR)/io.github.mrvladus.Errands.service.in $(BUILD_DIR)/$(APP_ID).service
	@sed -i "s/@APP_ID@/$(APP_ID)/g" $(BUILD_DIR)/$(APP_ID).service
	@sed -i "s|@BIN_DIR@|$(bindir)|g" $(BUILD_DIR)/$(APP_ID).service
	install -Dm 644 $(BUILD_DIR)/$(APP_ID).service $(DESTDIR)$(dbusdir)/$(APP_ID).service

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(NAME)
	rm -f $(DESTDIR)$(desktopdir)/$(APP_ID).desktop
	rm -f $(DESTDIR)$(scalableicondir)/$(APP_ID).svg
	rm -f $(DESTDIR)$(symbolicicondir)/io.github.mrvladus.Errands-symbolic.svg
	rm -f $(DESTDIR)$(dbusdir)/$(APP_ID).service

# --- Development targets --- #

RUN_CMD := $(BUILD_DIR)/$(NAME)

# Debug mode: GDB, GF2.
DEBUG ?=
ifeq ($(DEBUG),GDB)
    RUN_CMD := gdb -q -ex run $(RUN_CMD)
else ifeq ($(DEBUG),GF2)
    RUN_CMD := gf2 $(RUN_CMD)
endif

# Build and run the application
run: all
	@$(RUN_CMD)

# Count source lines of code
sloc:
	@find $(SRC_DIR) -name '*.c' -o -name '*.h' | sort | xargs wc -l

.PHONY: all install uninstall run clean sloc
