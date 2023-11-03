# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/shortcuts_window.ui")
class ShortcutsWindow(Gtk.ShortcutsWindow):
    __gtype_name__ = "ShortcutsWindow"

    def __init__(self, window: Adw.ApplicationWindow):
        super().__init__()
        self.set_transient_for(window)
        self.present()
