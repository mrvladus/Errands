# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT
from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GLib, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList


class TaskListContent(Gtk.ScrolledWindow):
    scrolling: bool = False

    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __repr__(self) -> str:
        return f"<class 'TaskListContent' {self.task_list.list_uid}>"

    def __build_ui(self):
        # Adjustment
        adj: Gtk.Adjustment = Gtk.Adjustment()
        adj.connect("value-changed", self._on_scroll)
        self.set_vadjustment(adj)

        # Drop controller
        drop_ctrl: Gtk.DropControllerMotion = Gtk.DropControllerMotion()
        drop_ctrl.connect("motion", self._on_dnd_scroll, adj)
        self.add_controller(drop_ctrl)

        # Uncompleted list
        self.uncompleted_task_list = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)

        # Completed list
        self.completed_task_list = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)

        # Task lists box
        box: Gtk.Box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, margin_bottom=32)
        box.append(self.uncompleted_task_list)
        box.append(self.completed_task_list)

        # Clamp
        clamp: Adw.Clamp = Adw.Clamp(
            tightening_threshold=300, maximum_size=1000, child=box
        )
        self.set_child(clamp)

    # ------ PUBLIC METHODS ------ #

    def update_ui(self, update_tasks: bool = True) -> None:
        # Update tasks
        tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.task_list.list_uid, "")
            if not t.deleted
        ]
        tasks_uids: list[str] = [t.uid for t in tasks]
        widgets_uids: list[str] = [t.uid for t in self.task_list.tasks]

        # Add tasks
        for task in tasks:
            if task.uid not in widgets_uids:
                self.add_task(task)

        for task in self.task_list.tasks:
            # Remove task
            if task.uid not in tasks_uids:
                task.purge()
            # Move task to completed tasks
            elif (
                task.get_prop("completed") and task in self.task_list.uncompleted_tasks
            ):
                if (
                    len(self.task_list.uncompleted_tasks) > 1
                    and task.uid != self.task_list.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.task_list.list_uid,
                        task.uid,
                        self.task_list.uncompleted_tasks[-1].uid,
                    )
                self.uncompleted_task_list.remove(task)
                self.completed_task_list.prepend(task)
            # Move task to uncompleted tasks
            elif (
                not task.get_prop("completed")
                and task in self.task_list.completed_tasks
            ):
                if (
                    len(self.task_list.uncompleted_tasks) > 0
                    and task.uid != self.task_list.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.task_list.list_uid,
                        task.uid,
                        self.task_list.uncompleted_tasks[-1].uid,
                    )
                self.completed_task_list.remove(task)
                self.uncompleted_task_list.append(task)

        # Update tasks
        if update_tasks:
            for task in self.task_list.tasks:
                task.update_ui()

    def _on_dnd_scroll(self, _motion, _x, y: float, adj: Gtk.Adjustment) -> None:
        def __auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.dnd_ctrl.contains_pointer():
                return False
            adj.set_value(adj.get_value() - (2 if scroll_up else -2))
            return True

        MARGIN: int = 50
        if y < MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, __auto_scroll, True)
        elif y > self.get_allocation().height - MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, __auto_scroll, False)
        else:
            self.scrolling = False

    def _on_scroll(self, adj: Gtk.Adjustment) -> None:
        self.task_list.header_bar.scroll_up_btn.set_visible(adj.get_value() > 0)
