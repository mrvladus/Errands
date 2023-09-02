#!/usr/bin/bash
echo -e "\n\033[32;1m---------- BUILDING ERRANDS ---------\033[0m\n"
flatpak run org.flatpak.Builder --user --install --force-clean --keep-build-dirs --ccache --disable-updates _build io.github.mrvladus.List.Devel.json
echo -e "\n\033[32;1m---------- RUNNING ERRANDS ----------\033[0m\n"
flatpak run io.github.mrvladus.List.Devel
echo -e "\n\033[32;1m------------- DONE! --------------\033[0m\n"
