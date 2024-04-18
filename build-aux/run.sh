#!/usr/bin/bash

cwd=$(pwd)

flatpak build-init $cwd/.flatpak/repo io.github.mrvladus.List.Devel org.gnome.Sdk org.gnome.Platform 46

flatpak build \
--share=network \
--filesystem=$cwd \
--filesystem=$cwd/.flatpak/repo \
--env=PATH=$PATH \
--env=LD_LIBRARY_PATH=/app/lib \
--env=PKG_CONFIG_PATH=/app/lib/pkgconfig:/app/share/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig \
--filesystem=$cwd/_build $cwd/.flatpak/repo ninja -C _build

flatpak build \
--share=network \
--filesystem=$cwd \
--filesystem=$cwd/.flatpak/repo \
--env=PATH=$PATH --env=LD_LIBRARY_PATH=/app/lib \
--env=PKG_CONFIG_PATH=/app/lib/pkgconfig:/app/share/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig \
--filesystem=$cwd/_build \ $cwd/.flatpak/repo meson install -C _build

echo "Run"
flatpak build \
--with-appdir \
--allow=devel \
--bind-mount=/run/user/1000/doc=/run/user/1000/doc/by-app/io.github.mrvladus.List.Devel \
--device=dri \
--socket=wayland \
--socket=fallback-x11 \
--share=ipc \
--share=network \
--talk-name=org.freedesktop.portal.* \
--talk-name=org.a11y.Bus \
--bind-mount=/run/flatpak/at-spi-bus=/run/user/1000/at-spi/bus \
--env=AT_SPI_BUS_ADDRESS=unix:path=/run/flatpak/at-spi-bus \
--bind-mount=/run/host/fonts=/usr/share/fonts \
--filesystem=$HOME/.local/share/fonts:ro \
--filesystem=$HOME/.cache/fontconfig:ro \
--bind-mount=/run/host/user-fonts-cache=$HOME/.cache/fontconfig \
--bind-mount=/run/host/font-dirs.xml=$HOME/.cache/font-dirs.xml $cwd/.flatpak/repo errands
