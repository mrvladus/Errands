# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
import os
from typing import TYPE_CHECKING


if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.data import TaskData, UserData
from gi.repository import Adw, Gtk, Gio  # type:ignore
from errands.lib.logging import Log


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TrashItem(Adw.ActionRow):
    __gtype_name__ = "TrashItem"

    def __init__(self, task: TaskData) -> None:
        super().__init__()
        self.task_dict: TaskData = task
        self.window: Window = Gio.Application.get_default().get_active_window()

    @Gtk.Template.Callback()
    def _on_restore_btn_clicked(self, _) -> None:
        """Restore task and its parents"""

        Log.info(f"Restore task: {self.task_dict['uid']}")

        parents_uids: list[str] = UserData.get_task_parents_uids_tree(
            self.task_dict["list_uid"], self.task_dict["uid"]
        )
        parents_uids.append(self.task_dict["uid"])
        for uid in parents_uids:
            UserData.update_props(self.task_dict["list_uid"], uid, ["trash"], [False])

        self.window.sidebar.update_ui()
