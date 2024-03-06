# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING
from errands.lib.gsettings import GSettings

from errands.lib.logging import Log

if TYPE_CHECKING:
    from errands.widgets.window import Window

import os
from gi.repository import Adw, Gtk  # type:ignore


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TodayRow(Gtk.ListBoxRow):
    __gtype_name__ = "TodayRow"

    def __init__(self) -> None:
        super().__init__()
        self.window: Window = Adw.Application.get_default().get_active_window()
        self.name: str = "errands_today_page"

    @Gtk.Template.Callback()
    def _on_row_activated(self, *args) -> None:
        Log.debug(f"Sidebar: Open Today")

        self.window.stack.set_visible_child_name(self.name)
        self.window.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", self.name)

    def update_ui(self) -> None:
        pass
