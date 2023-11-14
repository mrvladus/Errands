# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.logging import Log
from errands.widgets.task import Task
from gi.repository import Adw, Gtk


class TrashItem(Adw.ActionRow):
    def __init__(self, uid: str, tasks_panel, trash_list) -> None:
        super().__init__()
        self.uid = uid
        self.tasks_panel = tasks_panel
        self.trash_list = trash_list
        self.build_ui()

    def build_ui(self):
        self.set_title(UserData.get_prop(self.tasks_panel.list_uid, self.uid, "text"))
        restore_btn = Gtk.Button(
            tooltip_text=_("Restore"),  # type:ignore
            icon_name="emblem-ok-symbolic",
            valign="center",
            css_classes=["flat", "circular"],
        )
        # restore_btn.update_property([4], [_("Restore")])  # type:ignore
        restore_btn.connect("clicked", self.on_restore)
        self.add_suffix(restore_btn)

    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.uid}")

        tasks: list[Task] = self.tasks_panel.get_all_tasks()

        def restore_task(uid: str = self.uid) -> None:
            for task in tasks:
                if task.get_prop("uid") == uid:
                    task.update_prop("deleted", False)
                    task.toggle_visibility(True)
                    if task.get_prop("parent"):
                        task.parent.expand(True)
                        restore_task(task.get_prop("parent"))
                    break

        restore_task()
        self.tasks_panel.update_status()
        self.tasks_panel.trash_panel.trash_clear()
