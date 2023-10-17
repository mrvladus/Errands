# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
from gi.repository import Adw, Gtk, GLib, Gio
from .utils import Log


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/task_details.ui")
class TaskDetails(Adw.PreferencesWindow):
    __gtype_name__ = "TaskDetails"

    def __init__(self, task) -> None:
        super().__init__()
        self.task = task
        self.set_transient_for(task.window)
        self.present()

    def _delete_old_ics(self):
        pass

    def _convert_to_ics(self):
        ics_text: str = "hello"
        cache_dir: str = os.path.join(GLib.get_user_cache_dir(), "list")
        if not os.path.exists(cache_dir):
            os.mkdir(cache_dir)
        with open(os.path.join(cache_dir, f"{self.task.task['id']}.ics"), "w") as f:
            f.write(ics_text)

    @Gtk.Template.Callback()
    def on_calendar_open(self, _btn):
        self._convert_to_ics()
        file: Gio.File = Gio.File.new_for_path(
            os.path.join(
                GLib.get_user_cache_dir(), "list", f"{self.task.task['id']}.ics"
            )
        )
        Gtk.FileLauncher.new(file).launch()
