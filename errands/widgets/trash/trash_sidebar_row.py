# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os

from gi.repository import Gio, Gtk  # type:ignore

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.state import State
from errands.widgets.task.task import Task


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TrashSidebarRow(Gtk.ListBoxRow):
    __gtype_name__ = "TrashSidebarRow"

    size_counter: Gtk.Label = Gtk.Template.Child()
    icon: Gtk.Image = Gtk.Template.Child()
    menu_btn: Gtk.MenuButton = Gtk.Template.Child()

    def __init__(self) -> None:
        super().__init__()
        State.trash_sidebar_row = self
        self.__add_actions()

    def __add_actions(self) -> None:
        self.group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="trash_row", group=self.group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction(name=name)
            action.connect("activate", callback)
            self.group.add_action(action)

        __create_action("clear", lambda *_: State.trash_page.on_trash_clear())
        __create_action("restore", lambda *_: State.trash_page.on_trash_restore())

    def update_ui(self) -> None:
        # Update trash
        State.trash_page.update_ui()

        # Get trash size
        size: int = len(State.trash_page.trash_items)

        # Update actions state
        self.group.lookup_action("clear").set_enabled(size > 0)
        self.group.lookup_action("restore").set_enabled(size > 0)

        # Update icon name
        self.icon.set_from_icon_name(
            f"errands-trash{'-full' if size > 0 else ''}-symbolic"
        )

        # Update subtitle
        self.size_counter.set_label("" if size == 0 else str(size))

    @Gtk.Template.Callback()
    def _on_row_activated(self, *args) -> None:
        Log.debug("Sidebar: Open Trash")

        State.view_stack.set_visible_child_name("errands_trash_page")
        State.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", "errands_trash_page")

    @Gtk.Template.Callback()
    def _on_task_drop(self, _d, task: Task, _x, _y) -> None:
        task.delete()
