#!/usr/bin/env bash

if ! command -v distrobox-create >/dev/null 2>&1; then
    echo "Distrobox is not installed. Look for it here: https://github.com/89luca89/distrobox"
    exit 1
fi

distrobox-create \
    --name errands-distrobox \
    --image archlinux:latest \
    --init \
    --additional-packages "systemd gcc make pkgconf git neovim zed curl libadwaita libportal-gtk4 libical libsecret gtksourceview5 blueprint-compiler clang flatpak-builder"

if [ $? -ne 0 ]; then
    echo "Failed to create distrobox"
    exit 1
fi

distrobox-enter errands-distrobox
