# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk
from .utils import Log


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/task_details.ui")
class TaskDetails(Adw.PreferencesWindow):
    __gtype_name__ = "TaskDetails"

    def __init__(self, task) -> None:
        super().__init__()
        self.set_transient_for(task.window)
        self.present()

    def _convert_to_ics(self):
        pass

    @Gtk.Template.Callback()
    def on_calendar_open(self, _btn):
        pass
