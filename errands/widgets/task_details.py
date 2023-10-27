# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from email.policy import default
import os
import calendar
from errands.widgets.task import Task
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from gi.repository import Adw, Gtk, Gio, GLib, Gdk, GObject
from errands.utils.logging import Log
from errands.utils.tasks import task_to_ics


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/task_details.ui")
class TaskDetails(Adw.Window):
    __gtype_name__ = "TaskDetails"

    edit_entry: Adw.EntryRow = Gtk.Template.Child()
    notes: Gtk.TextView = Gtk.Template.Child()
    start_hour: Gtk.SpinButton = Gtk.Template.Child()
    start_min: Gtk.SpinButton = Gtk.Template.Child()
    start_date: Adw.ActionRow = Gtk.Template.Child()
    start_cal: Gtk.Calendar = Gtk.Template.Child()
    end_hour: Gtk.SpinButton = Gtk.Template.Child()
    end_min: Gtk.SpinButton = Gtk.Template.Child()
    end_date: Adw.ActionRow = Gtk.Template.Child()
    end_cal: Gtk.Calendar = Gtk.Template.Child()

    def __init__(self, parent: Task) -> None:
        super().__init__(transient_for=parent.window)
        self.parent = parent
        self._fill_info()
        self.present()

    def _fill_info(self):
        self.edit_entry.set_text(self.parent.task["text"])
        self.notes.get_buffer().set_text(self.parent.task["notes"])
        # Set date
        sd = self.parent.task["start_date"]
        self.start_hour.set_value(int(sd[9:11]))
        self.start_min.set_value(int(sd[11:13]))
        dt = GLib.DateTime.new_local(int(sd[0:4]), int(sd[4:6]), int(sd[6:8]), 0, 0, 0)
        self.start_cal.select_day(dt)
        ed = self.parent.task["end_date"]
        self.end_hour.set_value(int(ed[9:11]))
        self.end_min.set_value(int(ed[11:13]))
        dt = GLib.DateTime.new_local(int(ed[0:4]), int(ed[4:6]), int(ed[6:8]), 0, 0, 0)
        self.end_cal.select_day(dt)

    @Gtk.Template.Callback()
    def on_start_time_changed(self, _):
        hour = str(self.start_hour.get_value_as_int())
        if len(hour) == 1:
            hour = f"0{hour}"
        min = str(self.start_min.get_value_as_int())
        if len(min) == 1:
            min = f"0{min}"
        self.start_date.set_title(
            f"{hour}:{min}, {self.start_cal.get_date().format('%d %B, %Y')}"
        )

    @Gtk.Template.Callback()
    def on_end_time_changed(self, _):
        hour = str(self.end_hour.get_value_as_int())
        if len(hour) == 1:
            hour = f"0{hour}"
        min = str(self.end_min.get_value_as_int())
        if len(min) == 1:
            min = f"0{min}"
        self.end_date.set_title(
            f"{hour}:{min}, {self.end_cal.get_date().format('%d %B, %Y')}"
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
