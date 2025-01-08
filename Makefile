CC = gcc
CFLAGS = $(shell pkg-config --cflags libadwaita-1 gtksourceview-5 libcurl libical) -O3
LDFLAGS = $(shell pkg-config --libs libadwaita-1 gtksourceview-5 libcurl libical)

# Source files
SRC = $(shell find src -name '*.c')

# Object files
OBJ = $(SRC:.c=.o)

# Executable name
TARGET = errands

# Resource files and CSS files
RESOURCES = data/errands.gresource.xml data/styles/style.css data/styles/style-dark.css
RESOURCES_SRC = src/resources.h src/resources.c

# Default target
all: $(TARGET)

# Generate source resource file
src/resources.c: $(RESOURCES)
	glib-compile-resources data/errands.gresource.xml --generate-source --sourcedir=data --target=src/resources.c --c-name=errands

# Generate header resource file
src/resources.h: $(RESOURCES)
	glib-compile-resources data/errands.gresource.xml --generate-header --sourcedir=data --target=src/resources.h --c-name=errands


# Link object files to create the executable
$(TARGET): $(RESOURCES_SRC) $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the executable
run: $(TARGET)
	./$(TARGET)

# Clean up build files (excluding resource files)
clean:
	rm -f $(OBJ) $(TARGET)

# Phony targets
.PHONY: all clean run
