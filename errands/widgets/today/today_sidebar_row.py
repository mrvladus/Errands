# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING
from errands.lib.gsettings import GSettings

from errands.lib.logging import Log
from errands.widgets.task_list.task_list import TaskList, TaskListConfig

if TYPE_CHECKING:
    from errands.widgets.window import Window

import os
from gi.repository import Adw, Gtk  # type:ignore


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TodaySidebarRow(Gtk.ListBoxRow):
    __gtype_name__ = "TodaySidebarRow"

    def __init__(self) -> None:
        super().__init__()
        self.__setup_config()
        self.window: Window = Adw.Application.get_default().get_active_window()
        self.name: str = "errands_today_page"
        self.task_list = TaskList(self.config)
        self.page: Adw.ViewStackPage = self.window.stack.add_titled(
            self.task_list, self.name, _("Today")
        )

    def __setup_config(self):
        self.config = TaskListConfig(
            is_today_list=True,
            show_entry=False,
            title=_("Today"),
        )

    @Gtk.Template.Callback()
    def _on_row_activated(self, *args) -> None:
        Log.debug(f"Sidebar: Open Today")

        self.window.stack.set_visible_child_name(self.name)
        self.window.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", self.name)
        self.page.get_child().update_ui()

    def update_ui(self) -> None:
        pass
