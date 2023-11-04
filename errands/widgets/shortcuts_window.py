# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk


class ShortcutsWindow(Gtk.ShortcutsWindow):
    def __init__(self, window: Adw.ApplicationWindow):
        super().__init__()
        self.window = window
        self.build_ui()
        self.present()

    def build_ui(self):
        self.set_transient_for(self.window)
        section = Gtk.ShortcutsSection()
        self.add_section(section)
        general = Gtk.ShortcutsGroup(title=_("General"))  # type:ignore
        general.add_child(
            Gtk.ShortcutsShortcut(
                title=_("Show keyboard shortcuts"),  # type:ignore
                accelerator="<Control>question",
            ),
        )
        general.add_child(
            Gtk.ShortcutsShortcut(
                title=_("Toggle sidebar"),  # type:ignore
                accelerator="F9",
            ),
        )
        general.add_child(
            Gtk.ShortcutsShortcut(
                title=_("Preferences"),  # type:ignore
                accelerator="<Control>comma",
            ),
        )
        general.add_child(
            Gtk.ShortcutsShortcut(
                title=_("Quit"),  # type:ignore
                accelerator="<Control>q <Control>w",
            ),
        )
        section.add_group(general)
