#!/usr/bin/bash

SDK_VER=46
APP_ID=io.github.mrvladus.List.Devel
BIN_NAME=errands
CWD=$(pwd)
REPO_DIR=$CWD/.flatpak/repo
FLATPAK_BUILDER_DIR=$CWD/.flatpak/flatpak-builder
MANIFEST_JSON=$CWD/io.github.mrvladus.List.Devel.json


build() {
    echo "====== INIT REPO ======"
    flatpak build-init $REPO_DIR $APP_ID org.gnome.Sdk org.gnome.Platform $SDK_VER

    echo "====== BUILD 1 ======"
    flatpak run org.flatpak.Builder --ccache --force-clean --disable-updates --build-only --state-dir=$FLATPAK_BUILDER_DIR --stop-at=$BIN_NAME $REPO_DIR $MANIFEST_JSON --disable-rofiles-fuse

    echo "====== BUILD 2 ======"
    flatpak build --share=network --filesystem=$CWD --filesystem=$REPO_DIR --env=PATH=$PATH:/usr/lib64/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/app/bin:/usr/bin --env=LD_LIBRARY_PATH=/app/lib --env=PKG_CONFIG_PATH=/app/lib/pkgconfig:/app/share/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig --filesystem=$CWD/_build $REPO_DIR meson --prefix /app _build -Dprofile=development
}

run() {
    echo "====== RUN 1 ======"
    flatpak build --share=network --filesystem=$CWD --filesystem=$REPO_DIR --env=PATH=$PATH:/usr/lib64/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/app/bin:/usr/bin --env=LD_LIBRARY_PATH=/app/lib --env=PKG_CONFIG_PATH=/app/lib/pkgconfig:/app/share/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig --filesystem=$CWD/_build $REPO_DIR ninja -C _build

    echo "====== RUN 2 ======"
    flatpak build --share=network --filesystem=$CWD --filesystem=$REPO_DIR --env=PATH=$PATH:/usr/lib64/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/app/bin:/usr/bin --env=LD_LIBRARY_PATH=/app/lib --env=PKG_CONFIG_PATH=/app/lib/pkgconfig:/app/share/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig --filesystem=$CWD/_build $REPO_DIR meson install -C _build

    echo "====== RUN 3 ======"
    flatpak build --with-appdir --allow=devel --bind-mount=/run/user/$UID/doc=/run/user/$UID/doc/by-app/$APP_ID --device=dri --socket=wayland --socket=fallback-x11 --share=ipc --share=network --talk-name=org.freedesktop.secrets --talk-name=org.gnome.OnlineAccounts --talk-name=org.freedesktop.portal.* --talk-name=org.a11y.Bus --bind-mount=/run/flatpak/at-spi-bus=/run/user/$UID/at-spi/bus --env=AT_SPI_BUS_ADDRESS=unix:path=/run/flatpak/at-spi-bus --env=DESKTOP_SESSION=$DESKTOP_SESSION --env=LANG=$LANG --env=WAYLAND_DISPLAY=wayland-0 --env=XDG_CURRENT_DESKTOP=$XDG_CURRENT_DESKTOP --env=XDG_SESSION_DESKTOP=$XDG_SESSION_DESKTOP --env=XDG_SESSION_TYPE=$XDG_SESSION_TYPE --bind-mount=/run/host/fonts=/usr/share/fonts --bind-mount=/run/host/fonts-cache=/usr/lib/fontconfig/cache --filesystem=$HOME/.local/share/fonts:ro --filesystem=$HOME/.cache/fontconfig:ro --bind-mount=/run/host/user-fonts-cache=$HOME/.cache/fontconfig --bind-mount=/run/host/font-dirs.xml=$HOME/.cache/font-dirs.xml $REPO_DIR $BIN_NAME
}

rebuild() {
    echo "====== RE-BUILDING ======"
    rm -rf .flatpak _build
    build
    run
}

# Check if the first argument is "rebuild"
if [ "$1" = "rebuild" ]; then
    rebuild
else
    if [ -d "$REPO_DIR" ]; then
        run
    else
        build
        run
    fi
fi


