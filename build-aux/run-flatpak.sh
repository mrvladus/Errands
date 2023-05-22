#!/usr/bin/bash
flatpak run org.flatpak.Builder --user --install --force-clean _build io.github.mrvladus.List.yaml
flatpak run io.github.mrvladus.List
