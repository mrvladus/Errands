# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from gi.repository import Adw, GObject, Gtk, GLib  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.state import State
from errands.widgets.task_py.task_progress_bar import TaskProgressBar
from errands.widgets.task_py.task_tags_bar import TaskTagsBar
from errands.widgets.task_py.task_title import TaskTitle
from errands.widgets.task_py.task_top_drop_area import TaskTopDropArea


if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList


class Task(Adw.Bin):
    def __init__(self, task_data: TaskData, task_list: TaskList, parent) -> None:
        super().__init__()
        self.task_data = task_data
        self.list_uid = task_data.list_uid
        self.uid = task_data.uid
        self.task_list: TaskList = task_list
        self.parent = parent
        self.__build_ui()
        self.toggle_visibility(not task_data.trash)

    def __repr__(self) -> str:
        return f"<class 'Task' {self.task_data.uid}>"

    def __build_ui(self):
        # Top drop area
        self.top_drop_area = TaskTopDropArea(self)
        self.drop_controller: Gtk.DropControllerMotion = Gtk.DropControllerMotion()
        self.drop_controller.bind_property(
            "contains-pointer",
            self.top_drop_area,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE,
        )

        # Title
        self.title = TaskTitle(self)

        # Tags bar
        self.tags_bar = TaskTagsBar(self)

        # Progress bar
        self.progress_bar = TaskProgressBar(self)

        # Main box
        self.main_box: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            margin_start=12,
            margin_end=12,
            css_classes=["card", "fade"],
        )
        self.main_box.append(self.title)
        self.main_box.append(self.tags_bar)
        self.main_box.append(self.progress_bar)

        # Box
        box: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL, margin_bottom=6, margin_top=6
        )
        box.append(self.top_drop_area)
        box.append(self.main_box)

        # Revealer
        self.revealer = Gtk.Revealer(child=box)
        self.set_child(self.revealer)

    # ------ PUBLIC METHODS ------ #

    def add_task(self, task: TaskData) -> Task:
        Log.info(f"Task List: Add task '{task.uid}'")

        on_top: bool = GSettings.get("task-list-new-task-position-top")
        new_task = Task(task, self.task_list, self)
        if on_top:
            self.uncompleted_tasks_list.prepend(new_task)
        else:
            self.uncompleted_tasks_list.append(new_task)

    def add_rm_crossline(self, add: bool) -> None:
        if add:
            self.title_row.add_css_class("task-completed")
        else:
            self.title_row.remove_css_class("task-completed")

    def get_prop(self, prop: str) -> Any:
        return UserData.get_prop(self.list_uid, self.uid, prop)

    def get_status(self) -> tuple[int, int]:
        """Get total tasks and completed tasks tuple"""
        return UserData.get_status(self.list_uid, self.uid)

    def delete(self, *_) -> None:
        """Move task to trash"""

        Log.info(f"Task: Move to trash: '{self.uid}'")

        self.toggle_visibility(False)
        self.just_added = True
        self.complete_btn.set_active(True)
        self.just_added = False
        self.update_props(["trash", "completed", "synced"], [True, True, False])
        for task in self.all_tasks:
            task.just_added = True
            task.complete_btn.set_active(True)
            task.just_added = False
            task.update_props(["trash", "completed", "synced"], [True, True, False])
        State.today_page.update_ui()
        State.trash_sidebar_row.update_ui()
        State.tags_page.update_ui()
        self.task_list.update_status()

    def expand(self, expanded: bool) -> None:
        if expanded != self.get_prop("expanded"):
            self.update_props(["expanded"], [expanded])
        self.sub_tasks_revealer.set_reveal_child(expanded)
        if expanded:
            self.expand_indicator.remove_css_class("expand-indicator-expanded")
        else:
            self.expand_indicator.add_css_class("expand-indicator-expanded")

    def purge(self) -> None:
        """Completely remove widget"""

        if self.purging:
            return

        def __finish_remove():
            GLib.idle_add(self.get_parent().remove, self)
            return False

        self.purging = True
        self.toggle_visibility(False)
        GLib.timeout_add(300, __finish_remove)

    def update_props(self, props: list[str], values: list[Any]) -> None:
        # Update 'changed_at' if it's not in local props
        local_props: tuple[str] = (
            "deleted",
            "expanded",
            "synced",
            "toolbar_shown",
            "trash",
        )
        for prop in props:
            if prop not in local_props:
                props.append("changed_at")
                values.append(datetime.now().strftime("%Y%m%dT%H%M%S"))
                break
        UserData.update_props(self.list_uid, self.uid, props, values)
        # Update linked today task
        if props == ["expanded"] or props == ["toolbar_shown"]:
            State.today_page.update_ui()

    def toggle_visibility(self, on: bool) -> None:
        GLib.idle_add(self.revealer.set_reveal_child, on)

    def update_ui(self) -> None:
        self.title.update_ui()
