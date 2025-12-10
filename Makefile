# Project info

NAME = errands
APP_ID = io.github.mrvladus.Errands
VERSION = 49.0
VERSION_COMMIT = $(shell git rev-parse --short HEAD)

# Installation directories

DESTDIR ?=
prefix ?= /usr/local
bindir = $(prefix)/bin

# Project directories

BUILD_DIR = build
SRC_DIR = src
DATA_DIR = data

# Project sources

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)
BLPS = $(wildcard $(SRC_DIR)/*.blp)
CSS = $(wildcard $(DATA_DIR)/styles/*.css)
ICONS = $(wildcard $(DATA_DIR)/icons/*.svg)

# Compilation variables

CC = gcc
PKG_CONFIG_LIBS = libadwaita-1 gtksourceview-5 libical libportal libcurl
CFLAGS = -Wall -g \
		 `pkg-config --cflags $(PKG_CONFIG_LIBS)` \
		 -DVERSION='"$(VERSION)"' \
		 -DVERSION_COMMIT='"$(VERSION_COMMIT)"' \
		 -DAPP_ID='"$(APP_ID)"' \
	 	 -DLOCALE_DIR='""'
LDFLAGS = `pkg-config --libs $(PKG_CONFIG_LIBS)`

# Targets

all: $(BUILD_DIR)/$(NAME)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/$(NAME): $(OBJS)
	@echo "Linking $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/resources.c: $(DATA_DIR)/$(NAME).gresource.xml $(BLPS) $(CSS) $(ICONS)
	@echo "Building $@"
	@blueprint-compiler batch-compile $(BUILD_DIR) $(SRC_DIR) $(BLPS)
	@glib-compile-resources --generate-source --target=$@ --c-name=$(NAME) $<

install: $(BUILD_DIR)/$(NAME)
	install -d -s -m 755 $(BUILD_DIR)/$(NAME) $(DESTDIR)/$(bindir)/$(NAME)

uninstall:
	rm -f $(DESTDIR)/$(bindir)/$(NAME)

run: all
	@./$(BUILD_DIR)/$(NAME)

clean:
	@rm -rf $(BUILD_DIR)

-include $(DEPS)

.PHONY: all install uninstall run clean
