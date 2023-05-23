#!/usr/bin/bash
echo "---------- UPDATING TRANSLATIONS... ----------"
meson setup _build
cd _build
meson compile list-update-po
cd ..
echo "---------- DONE! ----------"
