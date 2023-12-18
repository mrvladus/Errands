# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.logging import Log
from errands.widgets.task import Task
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk


class TrashItem(Adw.Bin):
    def __init__(self, uid: str, trash) -> None:
        super().__init__()
        self.uid = uid
        self.trash = trash
        self.trash_list = trash.trash_list
        self.build_ui()

    def build_ui(self):
        self.add_css_class("card")
        row = Adw.ActionRow(
            title=UserData.get_prop(self.uid, "text"),
            css_classes=["rounded-corners"],
            height_request=60,
        )
        restore_btn = Gtk.Button(
            tooltip_text=_("Restore"),  # type:ignore
            icon_name="emblem-ok-symbolic",
            valign="center",
            css_classes=["flat", "circular"],
        )
        restore_btn.connect("clicked", self.on_restore)
        row.add_suffix(restore_btn)
        box = Gtk.ListBox(
            selection_mode=0,
            css_classes=["rounded-corners"],
            accessible_role=Gtk.AccessibleRole.PRESENTATION,
        )
        box.append(row)
        self.set_child(box)

    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.uid}")

        # Get all tasks
        tasks: list[Task] = []
        task_lists = []
        pages = self.trash.stack.get_pages()
        for i in range(pages.get_n_items()):
            child = pages.get_item(i).get_child()
            if hasattr(child, "get_all_tasks"):
                task_lists.append(child)
                tasks.extend(child.get_all_tasks())

        def restore_task(uid: str = self.uid) -> None:
            for task in tasks:
                if task.get_prop("uid") == uid:
                    task.update_props(["trash"], [False])
                    task.toggle_visibility(True)
                    if task.get_prop("parent"):
                        task.parent.expand(True)
                        restore_task(task.get_prop("parent"))
                    break

        restore_task()

        for list in task_lists:
            list.update_status()

        self.trash.trash_list.remove(self)
        self.trash.update_status()
