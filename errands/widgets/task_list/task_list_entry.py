# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.task_list.task_list import TaskList

from gi.repository import Adw, Gtk
from errands.lib.sync.sync import Sync
from errands.utils.animation import scroll
from errands.utils.data import UserData


class TaskListEntry(Adw.Bin):
    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list = task_list
        self._build_ui()

    def _build_ui(self):
        # Entry
        entry = Adw.EntryRow(
            activatable=False,
            height_request=60,
            title=_("Add new Task"),
        )
        entry.connect("entry-activated", self._on_task_added)
        # Box
        box = Gtk.ListBox(
            css_classes=["boxed-list"],
            selection_mode=Gtk.SelectionMode.NONE,
            margin_top=12,
            margin_bottom=12,
            margin_start=12,
            margin_end=12,
        )
        box.append(entry)
        # Clamp
        self.set_child(
            Adw.Clamp(
                maximum_size=1000,
                tightening_threshold=300,
                child=box,
            )
        )

    def _on_task_added(self, entry: Adw.EntryRow):
        text: str = entry.props.text
        if text.strip(" \n\t") == "":
            return
        self.task_list.add_task(
            UserData.add_task(list_uid=self.task_list.list_uid, text=text)
        )
        entry.props.text = ""
        scroll(self.task_list.scrl, True)
        Sync.sync()
