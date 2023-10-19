# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GLib, Gio, Gtk
from __main__ import APP_ID
from errands.utils.logging import Log


class GSettings:
    """Class for accessing gsettings"""

    gsettings: Gio.Settings = None
    initialized: bool = False

    def _check_init(self):
        if not self.initialized:
            self.init()

    @classmethod
    def bind(self, setting: str, obj: Gtk.Widget, prop: str) -> None:
        self._check_init(self)
        self.gsettings.bind(setting, obj, prop, 0)

    @classmethod
    def get(self, setting: str):
        self._check_init(self)
        return self.gsettings.get_value(setting).unpack()

    @classmethod
    def set(self, setting: str, gvariant: str, value) -> None:
        self._check_init(self)
        self.gsettings.set_value(setting, GLib.Variant(gvariant, value))

    @classmethod
    def init(self) -> None:
        Log.debug("Initialize GSettings")
        self.initialized = True
        self.gsettings = Gio.Settings.new(APP_ID)
