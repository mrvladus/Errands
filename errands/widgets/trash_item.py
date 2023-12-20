# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.logging import Log
from errands.widgets.task import Task
from gi.repository import Adw, Gtk


class TrashItem(Adw.Bin):
    def __init__(self, task_widget, trash) -> None:
        super().__init__()
        self.task_widget = task_widget
        self.uid = task_widget.uid
        self.trash = trash
        self.trash_list = trash.trash_list
        self.build_ui()

    def build_ui(self):
        self.add_css_class("card")
        row = Adw.ActionRow(
            title=UserData.get_prop(self.uid, "text"),
            subtitle=UserData.run_sql(
                f"SELECT name FROM lists WHERE uid = '{self.task_widget.list_uid}'",
                fetch=True,
            )[0][0],
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

        to_remove = []

        def restore_task(uid: str = self.uid) -> None:
            for item in get_children(self.trash_list):
                if item.task_widget.get_prop("uid") == uid:
                    item.task_widget.update_props(["trash"], [False])
                    item.task_widget.toggle_visibility(True)
                    to_remove.append(uid)
                    if puid := item.task_widget.get_prop("parent"):
                        item.task_widget.parent.expand(True)
                        item.task_widget.parent.update_status()
                        restore_task(puid)
                    break

        restore_task()

        for item in get_children(self.trash_list):
            if item.uid in to_remove:
                self.trash_list.remove(item)

        self.task_widget.task_list.update_status()
        self.trash.update_status()
