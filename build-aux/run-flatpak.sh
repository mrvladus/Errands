#!/usr/bin/bash
echo "---------- RUNNING LIST... ----------"
flatpak run org.flatpak.Builder --user --install --force-clean _build io.github.mrvladus.List.json
flatpak run io.github.mrvladus.List
echo "---------- DONE! ----------"
