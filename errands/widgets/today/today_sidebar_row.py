# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os

from gi.repository import Gtk  # type:ignore

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.state import State


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TodaySidebarRow(Gtk.ListBoxRow):
    __gtype_name__ = "TodaySidebarRow"

    size_counter: Gtk.Label = Gtk.Template.Child()

    def __init__(self) -> None:
        super().__init__()
        State.today_sidebar_row = self

    def update_ui(self):
        State.today_page.update_ui()

    @Gtk.Template.Callback()
    def _on_row_activated(self, *args) -> None:
        Log.debug("Sidebar: Open Today")
        State.view_stack.set_visible_child_name("errands_today_page")
        State.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", "errands_today_page")
