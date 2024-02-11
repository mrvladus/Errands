from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.application import ErrandsApplication

from abc import ABC, abstractmethod
import os
import subprocess
import sys
import importlib

from gi.repository import GLib  # type:ignore
from errands.lib.logging import Log


class PluginBase(ABC):

    @abstractmethod
    def initialize(self, app: ErrandsApplication) -> None: ...

    def run_tests(self): ...


class PluginsLoader:
    def __init__(self, app: ErrandsApplication) -> None:
        self.app = app
        Log.info(f"Plugins: Loading plugins...")

        self._load_plugins()

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
        plugin = importlib.import_module("plugin", path).Plugin
        plugin.initialize(self.app)

    def _load_plugins(self):
        sys.dont_write_bytecode = True
        for i in self._get_plugins_dirs(self._get_user_plugins_dir()):
            self._load_plugin(i["name"], i["dir"])
