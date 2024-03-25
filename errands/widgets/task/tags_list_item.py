# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.task.task import Task

import os
from gi.repository import Adw, Gtk, Gio  # type:ignore


# @Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TagsListItem(Gtk.ListBoxRow):
    # __gtype_name__ = "TagsListItem"

    def __init__(self, title: str, task: Task) -> None:
        super().__init__()
        self.set_activatable(True)
        self.task = task
        self.title = Gtk.Label(
            label=title,
            hexpand=True,
            halign="start",
        )
        box = Gtk.Box()
        box.append(self.title)
        box.append(
            Gtk.Image(icon_name="errands-tag-symbolic", css_classes=["dim-label"])
        )
        self.set_child(box)

    def do_activated(self) -> None:
        print(self.title.get_label())
