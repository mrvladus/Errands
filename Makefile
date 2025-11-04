NAME = errands
VERSION = 49.0
APP_ID = io.github.mrvladus.Errands

DESTDIR ?=
prefix ?= /usr/local
bindir = $(prefix)/bin

BUILD_DIR = build
SRC_DIR = src
DATA_DIR = data

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)
BLPS = $(wildcard $(SRC_DIR)/*.blp)

PKG_CONFIG_LIBS = libadwaita-1 gtksourceview-5 libical libportal libcurl webkitgtk-6.0

CFLAGS = -Wall -Wno-enum-int-mismatch -Wno-unknown-pragmas -g\
		 `pkg-config --cflags $(PKG_CONFIG_LIBS)` \
		 -DVERSION='"$(VERSION)"' \
		 -DAPP_ID='"$(APP_ID)"' \
	 	 -DLOCALE_DIR='""'

LDFLAGS = `pkg-config --libs $(PKG_CONFIG_LIBS)`

# ---------- TARGETS ---------- #

all: $(BUILD_DIR)/$(NAME)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/$(NAME): $(OBJS)
	@$(CC) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/resources.c: $(DATA_DIR)/$(NAME).gresource.xml $(BLPS)
	@blueprint-compiler batch-compile $(BUILD_DIR) $(SRC_DIR) $(BLPS)
	@glib-compile-resources --generate-source --target=$@ --c-name=$(NAME) $<

install: $(BUILD_DIR)/$(NAME)
	@mkdir -p $(DESTDIR)/$(bindir)
	cp $(BUILD_DIR)/$(NAME) $(DESTDIR)/$(bindir)/$(NAME)
	@strip $(DESTDIR)/$(bindir)/$(NAME)

uninstall:
	rm -f $(DESTDIR)/$(bindir)/$(NAME)

run: all
	@./$(BUILD_DIR)/$(NAME)

clean:
	@rm -rf $(BUILD_DIR)

-include $(DEPS)

.PHONY: all install uninstall run clean
