# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


import os
from datetime import datetime

from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.utils import get_children, idle_add
from errands.state import State
from errands.widgets.today.today_task import TodayTask


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Today(Adw.Bin):
    __gtype_name__ = "Today"

    status_page: Adw.StatusPage = Gtk.Template.Child()
    task_list: Gtk.ListBox = Gtk.Template.Child()

    def __init__(self):
        super().__init__()
        Log.debug("Today Page: Load")
        State.today_page = self
        self.__load_tasks()

    # ------ PRIVATE METHODS ------ #

    @idle_add
    def __load_tasks(self) -> None:
        Log.debug("Today Page: Load tasks for today")
        for task in self.tasks_data:
            new_task = TodayTask(task)
            self.task_list.append(new_task)
            new_task.update_ui()
        self.update_status()

    # ------ PROPERTIES ------ #

    @property
    def tasks(self) -> list[TodayTask]:
        return get_children(self.task_list)

    @property
    def tasks_data(self) -> list[TaskData]:
        return [
            t
            for t in UserData.tasks
            if not t.deleted
            and not t.trash
            and t.due_date
            and datetime.fromisoformat(t.due_date).date() == datetime.today().date()
        ]

    # ------ PUBLIC METHODS ------ #

    def update_status(self):
        """Update status and counter"""

        length: int = len(self.tasks_data)
        self.status_page.set_visible(length == 0)
        State.today_sidebar_row.size_counter.set_label(
            "" if length == 0 else str(length)
        )

    def update_ui(self):
        tasks = self.tasks_data
        tasks_uids: list[str] = [t.uid for t in tasks]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for task in tasks:
            if task.uid not in widgets_uids:
                new_task = TodayTask(task, self)
                self.task_list.append(new_task)
                new_task.update_ui()

        # Remove tasks
        for task in self.tasks:
            if task.uid not in tasks_uids:
                task.purge()

        for task in self.tasks:
            task.update_ui()

        self.update_status()
