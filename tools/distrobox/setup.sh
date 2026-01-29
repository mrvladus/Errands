#!/usr/bin/env bash

if ! command -v distrobox-create >/dev/null 2>&1; then
    echo "Installing Distrobox..."
    if command -v curl >/dev/null 2>&1; then
        curl -s https://raw.githubusercontent.com/89luca89/distrobox/main/install | sh -s -- --prefix ~/.local
    elif command -v wget >/dev/null 2>&1; then
        wget -qO- https://raw.githubusercontent.com/89luca89/distrobox/main/install | sh -s -- --prefix ~/.local
    else
        echo "Neither curl nor wget is installed. Please install one of them."
        exit 1
    fi
fi

distrobox-rm errands-distrobox --force

distrobox-create \
    --name errands-distrobox \
    --image archlinux:latest \
    --additional-packages "gcc make gdb pkgconf git neovim zed curl libadwaita libportal-gtk4 libical libsecret gtksourceview5 blueprint-compiler clang flatpak-builder"

if [ $? -ne 0 ]; then
    echo "Failed to create distrobox"
    exit 1
fi

distrobox-enter errands-distrobox
