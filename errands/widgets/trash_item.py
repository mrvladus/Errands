# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.logging import Log
from errands.widgets.task import Task
from gi.repository import Adw, Gtk


class TrashItem(Adw.ActionRow):
    def __init__(self, task: dict, tasks_panel, trash_list) -> None:
        super().__init__()
        self.task = task
        self.id: str = task["id"]
        self.tasks_panel = tasks_panel
        self.trash_list = trash_list
        self.build_ui()

    def build_ui(self):
        self.set_title(self.task["text"])
        restore_btn = Gtk.Button(
            tooltip_text=_("Restore"),  # type:ignore
            icon_name="emblem-ok-symbolic",
            valign="center",
            css_classes=["flat"],
        )
        # restore_btn.update_property([4], [_("Restore")])  # type:ignore
        restore_btn.connect("clicked", self.on_restore)
        self.add_suffix(restore_btn)

    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.id}")

        tasks: list[Task] = self.tasks_panel.get_all_tasks()

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
        self.tasks_panel.update_status()
        self.tasks_panel.trash_panel.trash_clear()
