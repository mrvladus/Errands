BIN = errands
VER = 49.0
APP_ID = io.github.mrvladus.Errands

BUILD_DIR := build
SRC_DIR = src
DATA_DIR = data

SRC = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/data/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

PKG_CONFIG_LIBS = libadwaita-1 gtksourceview-5 libical libportal libcurl webkitgtk-6.0

CFLAGS =	-Wall -Wno-enum-int-mismatch -Wno-unknown-pragmas \
			`pkg-config --cflags $(PKG_CONFIG_LIBS)` \
			-DVERSION='"$(VER)"' \
			-DAPP_ID='"$(APP_ID)"' \
			-DLOCALE_DIR='""'

LDFLAGS = `pkg-config --libs $(PKG_CONFIG_LIBS)`

# ---------- COMPILE TARGETS ---------- #

all: $(BUILD_DIR)/$(BIN)

-include $(DEP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# ---------- RESOURCES ---------- #

RESOURCES_C = $(SRC_DIR)/resources.c
GRESOURCES_XML = $(DATA_DIR)/errands.gresource.xml
BLP = $(wildcard $(SRC_DIR)/*.blp)

$(RESOURCES_C): $(GRESOURCES_XML) $(BLP)
	blueprint-compiler batch-compile $(BUILD_DIR) $(SRC_DIR) $(BLP)
	glib-compile-resources --generate-source --target=$(RESOURCES_C) --c-name=$(BIN) $(GRESOURCES_XML)

# ---------- PHONY ---------- #

run: $(BUILD_DIR)/$(BIN)
	./$(BUILD_DIR)/$(BIN)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
