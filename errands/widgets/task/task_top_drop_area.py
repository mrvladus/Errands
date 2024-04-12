# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Gtk, Gdk  # type:ignore

from errands.lib.data import UserData
from errands.lib.sync.sync import Sync


if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class TaskTopDropArea(Gtk.Revealer):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        from errands.widgets.task.task import Task

        image: Gtk.Image = Gtk.Image(
            icon_name="erraands-add-symbolic",
            margin_start=12,
            margin_end=12,
            css_classes=["task-drop-area"],
        )
        drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
            type=Task, actions=Gdk.DragAction.MOVE
        )
        drop_ctrl.connect("drop", self._on_task_drop)
        image.add_controller(drop_ctrl)
        self.set_child(image)

    # ------ SIGNAL HANDLERS ------ #

    def _on_task_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on "+" area on top of task
        """

        if task.list_uid != self.task.list_uid:
            UserData.move_task_to_list(
                task.uid,
                task.list_uid,
                self.task.list_uid,
                self.task.parent.uid if isinstance(self.task.parent, Task) else "",
            )
        UserData.move_task_before(self.task.list_uid, task.uid, self.task.uid)

        # If task completed and self is not completed - uncomplete task
        if task.complete_btn.get_active() and not self.task.complete_btn.get_active():
            task.update_props(["completed"], [False])
        elif not task.complete_btn.get_active() and self.task.complete_btn.get_active():
            task.update_props(["completed"], [True])

        # If task has the same parent box
        if task.parent == self.task.parent:
            box: Gtk.Box = self.get_parent()
            box.reorder_child_after(task, self)
            box.reorder_child_after(self, task)

        # Change parent if different parents
        else:
            UserData.update_props(
                self.task.list_uid,
                task.uid,
                ["parent", "synced"],
                [
                    self.task.parent.uid if isinstance(self.task.parent, Task) else "",
                    False,
                ],
            )

            # Toggle completion for parents
            if not task.get_prop("completed"):
                for parent in self.task.parents_tree:
                    parent.complete_btn.set_active(False)
            task.purge()

            new_task: Task = Task(
                UserData.get_task(self.task.list_uid, task.uid),
                self.task.task_list,
                self.task.parent,
            )
            box: Gtk.Box = self.get_parent()
            box.append(new_task)
            box.reorder_child_after(new_task, self)
            box.reorder_child_after(self, new_task)

        # KDE dnd bug workaround for issue #111
        for task in self.task.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

        # Update UI
        self.task.task_list.update_status()

        # Sync
        Sync.sync()
