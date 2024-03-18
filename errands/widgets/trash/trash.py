# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
import os
from typing import TYPE_CHECKING

from errands.widgets.trash.trash_item import TrashItem


if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.data import TaskData, TaskListData, UserDataSQLite
from errands.lib.utils import get_children
from errands.widgets.component import ConfirmDialog
from gi.repository import Adw, Gtk, GObject, Gio  # type:ignore
from errands.lib.sync.sync import Sync
from errands.lib.logging import Log


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Trash(Adw.Bin):
    __gtype_name__ = "Trash"

    status_page: Adw.StatusPage = Gtk.Template.Child()
    trash_list: Gtk.ListBox = Gtk.Template.Child()

    def __init__(self):
        super().__init__()
        self.window: Window = Gio.Application.get_default().get_active_window()
        self.stack: Adw.ViewStack = self.window.stack

    @property
    def trash_items(self) -> list[TrashItem]:
        return get_children(self.trash_list)

    def add_trash_item(self, uid: str) -> None:
        self.trash_list.append(TrashItem(uid))
        self.status_page.set_visible(False)

    def update_ui(self):
        tasks_dicts: list[TaskData] = [
            t
            for t in UserDataSQLite.get_tasks_as_dicts()
            if t["trash"] and not t["deleted"]
        ]
        tasks_uids: list[str] = [t["uid"] for t in tasks_dicts]
        lists_dicts: list[TaskListData] = UserDataSQLite.get_lists_as_dicts()

        # Add items
        items_uids = [t.task_dict["uid"] for t in self.trash_items]
        for t in tasks_dicts:
            if t["uid"] not in items_uids:
                self.add_trash_item(t)

        for item in self.trash_items:
            # Remove items
            if item.task_dict["uid"] not in tasks_uids:
                self.trash_list.remove(item)
                continue

            # Update title and subtitle
            task_dict = [t for t in tasks_dicts if t["uid"] == item.task_dict["uid"]][0]
            list_dict = [l for l in lists_dicts if l["uid"] == task_dict["list_uid"]][0]
            if item.get_title() != task_dict["text"]:
                item.set_title(task_dict["text"])
            if item.get_subtitle() != list_dict["name"]:
                item.set_subtitle(list_dict["name"])

        # Show status
        self.status_page.set_visible(len(self.trash_items) == 0)

    @Gtk.Template.Callback()
    def on_trash_clear(self, *args) -> None:
        def __confirm(_, res) -> None:
            if res == "cancel":
                Log.debug("Trash: Clear cancelled")
                return

            Log.info("Trash: Clear")

            UserDataSQLite.run_sql(
                f"""UPDATE tasks
                SET deleted = 1
                WHERE trash = 1""",
            )
            self.window.sidebar.trash_row.update_ui()
            # Sync
            Sync.sync()

        Log.debug("Trash: Show confirm dialog")

        ConfirmDialog(
            _("Tasks will be permanently deleted"),
            _("Delete"),
            Adw.ResponseAppearance.DESTRUCTIVE,
            __confirm,
        )

    @Gtk.Template.Callback()
    def on_trash_restore(self, *args) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Trash: Restore")

        trash_dicts: list[TaskData] = [
            t
            for t in UserDataSQLite.get_tasks_as_dicts()
            if t["trash"] and not t["deleted"]
        ]
        for task in trash_dicts:
            UserDataSQLite.update_props(
                task["list_uid"], task["uid"], ["trash"], [False]
            )

        self.window.sidebar.update_ui()
