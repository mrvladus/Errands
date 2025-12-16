# Project info

NAME = errands
APP_ID = io.github.mrvladus.Errands
VERSION = 49.0
VERSION_COMMIT = $(shell git rev-parse --short HEAD)

# Installation directories

DESTDIR ?=
prefix ?= /usr/local
bindir = $(prefix)/bin
datarootdir = $(prefix)/share
localedir = $(datarootdir)/locale
desktopdir = $(datarootdir)/applications

# Project directories

BUILD_DIR = build
SRC_DIR = src
DATA_DIR = data

# Resources

BLPS = $(wildcard $(SRC_DIR)/*.blp)
STYLES = $(wildcard $(DATA_DIR)/styles/*.css)
ICONS = $(wildcard $(DATA_DIR)/icons/*.svg)
GRESOURCE_XML = $(DATA_DIR)/$(NAME).gresource.xml
RESOURCES_C = $(BUILD_DIR)/resources.c
RESOURCES_O = $(BUILD_DIR)/resources.o

# Sources

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS)) $(RESOURCES_O)
DEPS = $(OBJS:.o=.d)

# Compilation variables

CC = gcc
PKG_CONFIG_LIBS = libadwaita-1 gtksourceview-5 libical libportal libcurl libsecret-1
CFLAGS = -Wall -g
ALL_CFLAGS = $(CFLAGS) \
			`pkg-config --cflags $(PKG_CONFIG_LIBS)` \
			-DVERSION='"$(VERSION)"' \
			-DVERSION_COMMIT='"$(VERSION_COMMIT)"' \
			-DAPP_ID='"$(APP_ID)"' \
			-DLOCALE_DIR='""'
LDFLAGS = `pkg-config --libs $(PKG_CONFIG_LIBS)`

# Targets

all: $(BUILD_DIR)/$(NAME)

clean:
	@rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Resources targets

$(RESOURCES_O): $(RESOURCES_C) | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(ALL_CFLAGS) -MMD -MP -c -o $@ $<

$(RESOURCES_C): $(GRESOURCE_XML) $(BLPS) $(STYLES) $(ICONS)
	@echo "Embedding resources into $@"
	@blueprint-compiler batch-compile $(BUILD_DIR) $(SRC_DIR) $(BLPS)
	@glib-compile-resources --generate-source --target=$@ --c-name=$(NAME) $<

# Sources targets

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(ALL_CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/$(NAME): $(OBJS)
		@echo "Linking executable $@"
		@$(CC) -o $@ $^ $(LDFLAGS)

-include $(DEPS)

# Installation related targets

install: $(BUILD_DIR)/$(NAME)
	# Executable
	install -Dsm 755 $(BUILD_DIR)/$(NAME) $(DESTDIR)/$(bindir)/$(NAME)
	# Desktop file
	cp $(DATA_DIR)/io.github.mrvladus.Errands.desktop.in $(BUILD_DIR)/$(APP_ID).desktop
	sed -i "s/APP_ID/$(APP_ID)/g" $(BUILD_DIR)/$(APP_ID).desktop
	install -Dm 755 $(BUILD_DIR)/$(DESKTOP_FILE) $(DESTDIR)/$(desktopdir)/$(APP_ID).desktop

uninstall:
	rm -f $(DESTDIR)/$(bindir)/$(NAME)
	rm -f $(DESTDIR)/$(desktopdir)/$(APP_ID).desktop

# Development targets

run: all
	@./$(BUILD_DIR)/$(NAME)

count-sloc:
	@find $(SRC_DIR) -name '*.c' -o -name '*.h' | sort | xargs wc -l

.PHONY: all install uninstall run clean count-sloc
