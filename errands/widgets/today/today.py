# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
import datetime
import os


from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.widgets.window import Window


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Today(Adw.Bin):
    __gtype_name__ = "Today"

    window = GObject.Property(type=Window)

    tasks_list: Gtk.Box = Gtk.Template.Child()

    def __init__(self):
        super().__init__()

    def update_ui(self):
        Log.debug("Today: Update UI")
        today = datetime.datetime.today().day

        # tasks: list[TaskData] = [
        #     t
        #     for t in UserData.tasks
        #     if t.due_date and datetime.datetime.fromisoformat(t.due_date).day == today
        # ]
        # for task in tasks:
        #     self.tasks_list.append(TodayTask(task))
