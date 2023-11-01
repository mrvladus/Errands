# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk
from errands.utils.logging import Log
from errands.widgets.task import Task


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/trash_item.ui")
class TrashItem(Gtk.Box):
    __gtype_name__ = "TrashItem"

    label: Gtk.Label = Gtk.Template.Child()

    def __init__(self, task: dict, window: Adw.ApplicationWindow) -> None:
        super().__init__()
        self.window: Adw.ApplicationWindow = window
        self.id: str = task["id"]
        self.label.set_label(task["text"])

    def __repr__(self) -> str:
        return f"TrashItem({self.id})"

    @Gtk.Template.Callback()
    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.id}")

        tasks: list[Task] = self.window.get_all_tasks()

        def restore_task(id: str = self.id) -> None:
            for task in tasks:
                if task.task["id"] == id:
                    task.task["deleted"] = False
                    task.update_data()
                    task.toggle_visibility(True)
                    if task.task["parent"]:
                        task.parent.expand(True)
                        restore_task(task.task["parent"])
                    break

        restore_task()
        self.window.update_status()
        self.window.trash_panel.trash_clear()
