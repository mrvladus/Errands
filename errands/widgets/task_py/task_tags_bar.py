# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Gdk, Gtk  # type:ignore

from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class TaskTagsBar(Gtk.Revealer):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.update_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        self.box: Gtk.FlowBox = Gtk.FlowBox(
            height_request=20,
            margin_start=12,
            margin_end=12,
            margin_bottom=3,
            selection_mode=Gtk.SelectionMode.NONE,
            max_children_per_line=1000,
        )
        self.set_child(self.box)

    # ------ PROPERTIES ------ #

    @property
    def tags(self) -> list[TaskTagsBarTag]:
        return [t.get_child() for t in get_children(self.box)]

    # ------ PUBLIC METHODS ------ #

    def add_tag(self, tag: str) -> None:
        self.box.append(TaskTagsBarTag(tag, self))

    def update_ui(self) -> None:
        tags: str = self.task.task_data.tags
        tags_list_text: list[str] = [tag.title for tag in self.tags]

        # Delete tags
        for tag in self.tags:
            if tag.title not in tags:
                self.box.remove(tag)

        # Add tags
        for tag in tags:
            if tag not in tags_list_text:
                self.add_tag(tag)

        self.set_reveal_child(tags != [])


class TaskTagsBarTag(Gtk.Box):
    def __init__(self, title: str, tags_bar: TaskTagsBar):
        super().__init__()
        self.title = title
        self.tags_bar = tags_bar
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        self.set_hexpand(False)
        self.set_valign(Gtk.Align.CENTER)
        self.add_css_class("tag")

        # Text
        text: Gtk.Label = Gtk.Label(
            label=self.title,
            css_classes=["caption-heading"],
            max_width_chars=15,
            ellipsize=3,
            hexpand=True,
            halign=Gtk.Align.START,
        )
        self.append(text)

        # Delete button
        delete_btn = Gtk.Button(
            icon_name="errands-close-symbolic",
            cursor=Gdk.Cursor(name="pointer"),
            tooltip_text=_("Delete Tag"),  # noqa: F821
        )
        delete_btn.connect("clicked", self._on_delete_btn_clicked)
        self.append(delete_btn)

    # ------ SIGNAL HANDLERS ------ #

    def _on_delete_btn_clicked(self, _btn: Gtk.Button) -> None:
        tags: list[str] = self.tags_bar.task.task_data.tags
        tags.remove(self.title)
        self.tags_bar.task.update_props(["tags", "synced"], [tags, False])
        self.tags_bar.box.remove(self)
        Sync.sync()
