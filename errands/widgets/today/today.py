# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from __future__ import annotations

from typing import TYPE_CHECKING


if TYPE_CHECKING:
    from errands.widgets.today.today_sidebar_row import TodaySidebarRow

import os
from datetime import datetime

from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.utils import get_children
from errands.widgets.today.today_task import TodayTask


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Today(Adw.Bin):
    __gtype_name__ = "Today"

    status_page: Adw.StatusPage = Gtk.Template.Child()
    task_list: Gtk.ListBox = Gtk.Template.Child()

    def __init__(self, today_sidebar_row: TodaySidebarRow):
        super().__init__()
        self.sidebar_row: TodaySidebarRow = today_sidebar_row

    @property
    def tasks(self) -> list[TodayTask]:
        return get_children(self.task_list)

    def update_ui(self):
        tasks: list[TaskData] = [
            t
            for t in UserData.tasks
            if not t.deleted
            and not t.trash
            and t.due_date
            and datetime.fromisoformat(t.due_date).date() == datetime.today().date()
        ]
        tasks_uids: list[str] = [t.uid for t in tasks]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for task in tasks:
            if task.uid not in widgets_uids:
                new_task = TodayTask(task)
                self.task_list.append(new_task)
                new_task.update_ui()

        # Remove tasks
        for task in self.tasks:
            if task.uid not in tasks_uids:
                task.purge()

        self.status_page.set_visible(len(tasks) == 0)
