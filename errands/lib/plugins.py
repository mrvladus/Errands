from __future__ import annotations
from abc import ABC
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.application import ErrandsApplication

import os
import subprocess
import sys
import importlib

from gi.repository import Gtk, GLib, Gdk  # type:ignore
from errands.lib.logging import Log


class PluginBase(ABC):
    author: str  # Name of the author
    description: str  # Short description
    icon_name: str  # Icon that shows beside name
    main_view: Gtk.Widget  # Main widget to put inside Errands view stack
    name: str  # Name of the plugin
    url: str  # Repo url


class PluginsLoader:
    plugins: list[PluginBase] = []

    def __init__(self, app: ErrandsApplication) -> None:
        self.app = app
        self._load_plugins()

    def _add_resources_path(self, dir):
        res_path: str = os.path.join(dir, "resources")
        if os.path.exists(res_path):
            # Icons
            icon_theme = Gtk.IconTheme.get_for_display(Gdk.Display.get_default())
            icon_theme.add_search_path(os.path.join(res_path, "icons"))

    def _get_user_plugins_dir(self) -> str:
        """Get plugins directory. Create if needed."""

        plugin_dir: str = os.path.join(GLib.get_user_data_dir(), "errands", "plugins")
        sys.path.insert(1, plugin_dir)
        if not os.path.exists(plugin_dir):
            os.mkdir(plugin_dir)

        return plugin_dir

    def _get_plugins_dirs(self, plugin_dir: str) -> list[dict[str, str]]:
        """Get all dirs that contains 'plugin.py' file."""

        plugin_names: list[str] = os.listdir(plugin_dir)
        plugins_dirs: list[dict[str, str]] = []
        for name in plugin_names:
            dir = os.path.join(plugin_dir, name)
            if os.path.exists(os.path.join(dir, "plugin.py")):
                plugins_dirs.append({"name": name, "dir": dir})
                sys.path.insert(1, dir)

        return plugins_dirs

    def _install_plugin_deps(self, plugin_dir: str):
        """
        If dir contains 'requirements.txt' run pip to install deps to 'dependencies' dir.
        """

        requirements_path: str = os.path.join(plugin_dir, "requirements.txt")
        if not os.path.exists(requirements_path):
            return

        dependencies_path: str = os.path.join(plugin_dir, "dependencies")
        if os.path.exists(dependencies_path):
            sys.path.insert(1, dependencies_path)
        else:
            os.mkdir(dependencies_path)
            sys.path.insert(1, dependencies_path)
            subprocess.check_call(
                [
                    sys.executable,
                    "-m",
                    "pip",
                    "install",
                    "-t",
                    dependencies_path,
                    "-r",
                    requirements_path,
                ]
            )

    def _load_plugin(self, name: str, path: str) -> None:
        Log.info(f"Plugins: Loading {name}")
        self._install_plugin_deps(path)
        plugin: PluginBase = importlib.import_module("plugin", path).Plugin()
        self.plugins.append(plugin)

    def _load_plugins(self):
        sys.dont_write_bytecode = True
        for i in self._get_plugins_dirs(self._get_user_plugins_dir()):
            self._add_resources_path(i["dir"])
            self._load_plugin(i["name"], i["dir"])
