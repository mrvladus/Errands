# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os

from gi.repository import Gtk  # type:ignore

from errands.lib.data import UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.state import State


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TagsSidebarRow(Gtk.ListBoxRow):
    __gtype_name__ = "TagsSidebarRow"

    size_counter: Gtk.Button = Gtk.Template.Child()

    def __init__(self) -> None:
        super().__init__()
        State.tags_sidebar_row = self
        self.update_ui()

    def update_ui(self) -> None:
        size: int = len(UserData.tags)
        self.size_counter.set_label(str(size) if size > 0 else "")

    @Gtk.Template.Callback()
    def _on_row_activated(self, *args) -> None:
        Log.debug("Sidebar: Open Tags")

        State.view_stack.set_visible_child_name("errands_tags_page")
        State.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", "errands_tags_page")
        State.tags_page.update_ui()
