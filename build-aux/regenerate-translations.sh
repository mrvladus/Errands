#!/usr/bin/bash
flatpak run --filesystem=home org.gnome.Sdk//44 <<EOF
echo -e "\n\033[32;1m---------- UPDATING TRANSLATIONS ----------\033[0m\n"
cd _build
meson compile list-update-po
echo -e "\n\033[32;1m------------------ DONE! ------------------\033[0m\n"
EOF