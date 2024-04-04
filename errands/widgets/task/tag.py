# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Gdk, Gtk

from errands.lib.sync.sync import Sync  # type:ignore

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class Tag(Gtk.Box):
    def __init__(self, title: str, task: Task):
        super().__init__()
        self.title = title
        self.task = task
        self.set_hexpand(False)
        self.set_valign(Gtk.Align.CENTER)
        self.add_css_class("tag")
        btn = Gtk.Button(
            icon_name="errands-close-symbolic", cursor=Gdk.Cursor(name="pointer")
        )
        btn.connect("clicked", self.delete)
        self.append(
            Gtk.Label(
                label=title,
                css_classes=["caption-heading"],
                max_width_chars=15,
                ellipsize=3,
                hexpand=True,
                halign=Gtk.Align.START,
            )
        )
        self.append(btn)

    def delete(self, _btn):
        tags: list[str] = self.task.get_prop("tags")
        tags.remove(self.title)
        self.task.update_props(["tags", "synced"], [tags, False])
        self.task.update_tags()
        Sync.sync(False)
