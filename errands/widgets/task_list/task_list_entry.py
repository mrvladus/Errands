# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw  # type:ignore

from errands.lib.animation import scroll
from errands.lib.data import UserData
from errands.lib.gsettings import GSettings
from errands.lib.sync.sync import Sync

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList


class TaskListEntry(Adw.Bin):
    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.__build_ui()

    def __repr__(self) -> str:
        return f"<class 'TaskListEntry' {self.task_list.list_uid}>"

    def __build_ui(self):
        entry: Adw.EntryRow = Adw.EntryRow(
            margin_top=3,
            margin_bottom=3,
            margin_end=12,
            margin_start=12,
            title=_("Add new Task"),
            activatable=False,
            height_request=60,
            css_classes=["card"],
        )
        entry.connect("entry-activated", self._on_task_added)

        clamp: Adw.Clamp = Adw.Clamp(
            maximum_size=1000, tightening_threshold=300, child=entry
        )
        self.set_child(clamp)

    def _on_task_added(self, entry: Adw.EntryRow) -> None:
        text: str = entry.get_text()
        if text.strip(" \n\t") == "":
            return
        self.task_list.add_task(
            UserData.add_task(
                list_uid=self.task_list.list_uid,
                text=text,
            )
        )
        entry.set_text("")
        if not GSettings.get("task-list-new-task-position-top"):
            scroll(self.task_list.content, True)

        self.task_list.header_bar.update_ui()
        Sync.sync()
