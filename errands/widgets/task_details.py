# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
import calendar
from errands.widgets.task import Task
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from gi.repository import Adw, Gtk, Gio, GLib, Gdk
from errands.utils.logging import Log
from errands.utils.tasks import task_to_ics


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/task_details.ui")
class TaskDetails(Adw.Window):
    __gtype_name__ = "TaskDetails"

    edit_entry: Adw.EntryRow = Gtk.Template.Child()
    start_date: Adw.ActionRow = Gtk.Template.Child()

    def __init__(self, parent: Task) -> None:
        super().__init__(transient_for=parent.window)
        self.parent = parent
        self._fill_info()
        self.present()

    def _fill_info(self):
        self.edit_entry.set_text(self.parent.task["text"])
        # Start date
        sd = self.parent.task.get("start_date", None)
        if sd:
            year = sd[0:4]
            day = sd[6:8]
            month = sd[4:6]
            self.start_date.set_title(
                f"{day} {calendar.month_name[int(month)]}, {year}"
            )

    @Gtk.Template.Callback()
    def on_copy_text_clicked(self, _btn):
        Log.info("Copy to clipboard")
        clp: Gdk.Clipboard = Gdk.Display.get_default().get_clipboard()
        clp.set(self.parent.task["text"])
        self.parent.window.add_toast(_("Copied to Clipboard"))  # pyright:ignore
        self.close()

    @Gtk.Template.Callback()
    def on_move_to_trash_clicked(self, _btn):
        self.parent.delete()
        self.close()

    @Gtk.Template.Callback()
    def on_open_as_ics_clicked(self, _btn):
        cache_dir: str = os.path.join(GLib.get_user_cache_dir(), "tmp")
        if not os.path.exists(cache_dir):
            os.mkdir(cache_dir)
        file_path = os.path.join(cache_dir, f"{self.parent.task['id']}.ics")
        with open(file_path, "w") as f:
            f.write(task_to_ics(self.parent.task))
        file: Gio.File = Gio.File.new_for_path(file_path)
        Gtk.FileLauncher.new(file).launch()

    @Gtk.Template.Callback()
    def on_start_date_selected(self, cal: Gtk.Calendar):
        self.start_date.set_title(cal.get_date().format("%d %B, %Y"))
        time: str = cal.get_date().format("%Y%m%dT000000")

    @Gtk.Template.Callback()
    def on_style_selected(self, btn: Gtk.Button) -> None:
        """
        Apply accent color
        """

        for i in btn.get_css_classes():
            color = ""
            if i.startswith("btn-"):
                color = i.split("-")[1]
                break
        # Color card
        for c in self.parent.main_box.get_css_classes():
            if "task-" in c:
                self.parent.main_box.remove_css_class(c)
                break
        self.parent.main_box.add_css_class(f"task-{color}")
        self.parent.task["color"] = color
        self.parent.task["synced_caldav"] = False
        self.parent.update_data()
        Sync.sync()

    @Gtk.Template.Callback()
    def on_text_edited(self, entry: Adw.EntryRow):
        new_text: str = entry.get_text()
        if new_text.strip(" \n\t") == "" or new_text == self.parent.task["text"]:
            entry.set_text(self.parent.task["text"])
            return
        Log.info(f"Edit: {self.parent.task['id']}")
        self.parent.task_row.set_title(Markup.find_url(Markup.escape(new_text)))
        self.parent.task["text"] = new_text
        self.parent.task["synced_caldav"] = False
        self.parent.update_data()
        Sync.sync()
