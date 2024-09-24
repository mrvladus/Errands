CC = gcc
CFLAGS = $(shell pkg-config --cflags libadwaita-1 gtksourceview-5) -O3
LDFLAGS = $(shell pkg-config --libs libadwaita-1 gtksourceview-5)

# Source files
SRC = $(wildcard src/*.c)

# Object files
OBJ = $(SRC:.c=.o)

# Executable name
TARGET = errands

# Default target
all: $(TARGET)

# Generate resource files from resource.xml
gen-res:
	glib-compile-resources data/errands.gresource.xml --generate-header --sourcedir=data --target=src/resources.h --c-name=errands
	glib-compile-resources data/errands.gresource.xml --generate-source --sourcedir=data --target=src/resources.c --c-name=errands

# Link object files to create the executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the executable
run: $(TARGET)
	./$(TARGET)

# Clean up build files
clean:
	rm -f $(OBJ) $(TARGET)

# Phony targets
.PHONY: all clean run gen-res
