# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os
from typing import TYPE_CHECKING

from gi.repository import Adw, Gtk  # type:ignore

# from errands.lib.logging import Log
from errands.widgets.components.datetime_picker import DateTimePicker

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class DateTimeWindow(Adw.Dialog):
    __gtype_name__ = "DateTimeWindow"

    start_date_time: DateTimePicker = Gtk.Template.Child()
    due_date_time: DateTimePicker = Gtk.Template.Child()

    def __init__(self, task: Task):
        super().__init__()
        self.task = task

    def show(self):
        self.start_date_time.datetime = self.task.get_prop("start_date")
        self.due_date_time.datetime = self.task.get_prop("due_date")
        self.present(self.task.window)

    def do_closed(self):
        pass

    @Gtk.Template.Callback()
    def _on_date_time_start_set(self, *args) -> None:
        self.task.update_props(["start_date"], [self.start_date_time.datetime])

    @Gtk.Template.Callback()
    def _on_date_time_due_set(self, *args) -> None:
        self.task.update_props(["due_date"], [self.due_date_time.datetime])
        self.task.update_toolbar()
