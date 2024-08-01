# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, Gtk, GObject  # type:ignore

from errands.lib.data import UserData
from errands.lib.logging import Log
from errands.lib.utils import get_children
from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsCheckButton
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView

if TYPE_CHECKING:
    from errands.widgets.task import Task
    from errands.widgets.today.today_task import TodayTask


class TagWindow(Adw.Dialog):
    def __init__(self):
        super().__init__()
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_follows_content_size(True)
        self.set_title(_("Tags"))

        tags_status_page: Adw.StatusPage = Adw.StatusPage(
            title=_("No Tags"),
            icon_name="errands-info-symbolic",
            css_classes=["compact"],
        )
        self.tags_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=6,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
            margin_top=6,
        )
        self.tags_list.bind_property(
            "visible",
            tags_status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[Adw.HeaderBar()],
                content=Gtk.ScrolledWindow(
                    width_request=200,
                    propagate_natural_height=True,
                    propagate_natural_width=True,
                    vexpand=True,
                    child=ErrandsBox(
                        orientation=Gtk.Orientation.VERTICAL,
                        vexpand=True,
                        valign=Gtk.Align.CENTER,
                        children=[self.tags_list, tags_status_page],
                    ),
                ),
            )
        )

    # ------ PUBLIC METHODS ------ #

    def show(self, task: Task | TodayTask):
        self.task = task
        tags: list[str] = [t.text for t in UserData.tags]
        tags_list_items: list[ErrandsToolbarTagsListItem] = get_children(self.tags_list)
        tags_list_items_text = [t.title for t in tags_list_items]

        # Remove tags
        for item in tags_list_items:
            if item.title not in tags:
                self.tags_list.remove(item)

        # Add tags
        for tag in tags:
            if tag not in tags_list_items_text:
                self.tags_list.append(ErrandsToolbarTagsListItem(tag, self))

        # Toggle tags
        task_tags: list[str] = [t.title for t in self.task.tags]
        tags_items: list[ErrandsToolbarTagsListItem] = get_children(self.tags_list)
        for t in tags_items:
            t.block_signals = True
            t.toggle_btn.set_active(t.title in task_tags)
            t.block_signals = False

        self.tags_list.set_visible(len(get_children(self.tags_list)) > 0)
        self.present(State.main_window)


class ErrandsToolbarTagsListItem(Gtk.Box):
    block_signals = False

    def __init__(self, title: str, tag_window: TagWindow) -> None:
        super().__init__()
        self.set_spacing(6)
        self.title = title
        self.tag_window = tag_window
        self.toggle_btn = ErrandsCheckButton(on_toggle=self.__on_toggle)
        self.append(self.toggle_btn)
        self.append(
            Gtk.Label(
                label=title, hexpand=True, halign=Gtk.Align.START, max_width_chars=20
            )
        )
        self.append(
            Gtk.Image(icon_name="errands-tag-symbolic", css_classes=["dim-label"])
        )

    def __on_toggle(self, btn: Gtk.CheckButton) -> None:
        if self.block_signals:
            return
        task = self.tag_window.task

        tags: list[str] = task.task_data.tags

        if btn.get_active():
            if self.title not in tags:
                tags.append(self.title)
        else:
            if self.title in tags:
                tags.remove(self.title)

        task.update_props(["tags", "synced"], [tags, False])
        task.update_tags_bar()
        Sync.sync()
