# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.logging import Log
from errands.widgets.task import Task
from gi.repository import Adw, Gtk


class TrashItem(Adw.ActionRow):
    def __init__(self, task: dict, tasks_list) -> None:
        super().__init__()
        self.task = task
        self.id: str = task["id"]
        self.tasks_list = tasks_list
        self.build_ui()

    def build_ui(self):
        self.set_title(self.task["text"])
        restore_btn = Gtk.Button(
            tooltip_text=_("Restore"),  # type:ignore
            icon_name="emblem-ok-symbolic",
            valign="center",
        )
        # restore_btn.update_property([4], [_("Restore")])  # type:ignore
        restore_btn.connect("clicked", self.on_restore)
        self.add_suffix(restore_btn)

    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.id}")

        tasks: list[Task] = self.tasks_list.get_all_tasks()

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
        self.tasks_list.update_status()
        self.tasks_list.trash_panel.trash_clear()
