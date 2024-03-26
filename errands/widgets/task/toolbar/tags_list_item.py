# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.task.task import Task

from gi.repository import Gtk  # type:ignore


class TagsListItem(Gtk.Box):
    block_signals = False

    def __init__(self, title: str, task: Task) -> None:
        super().__init__()
        self.set_spacing(6)
        self.title = title
        self.task = task
        self.toggle_btn = Gtk.CheckButton()
        self.toggle_btn.connect("toggled", self.__on_toggle)
        self.append(self.toggle_btn)
        self.append(
            Gtk.Label(
                label=title,
                hexpand=True,
                halign="start",
            )
        )

    def __on_toggle(self, btn: Gtk.CheckButton) -> None:
        if self.block_signals:
            return
        tags: str = self.task.get_prop("tags")
        if tags != "":
            tags: list[str] = tags.split(",")
        else:
            tags = []

        if btn.get_active():
            if self.title not in tags:
                tags.append(self.title)
        else:
            if self.title in tags:
                tags.remove(self.title)

        self.task.update_props(["tags", "synced"], [",".join(tags), False])
        self.task.update_tags()
