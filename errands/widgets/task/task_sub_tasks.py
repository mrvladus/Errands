# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children


if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class TaskSubTasks(Gtk.Revealer):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.__load_sub_tasks()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        # Entry
        entry: Gtk.Entry = Gtk.Entry(
            margin_start=12,
            margin_end=12,
            margin_top=3,
            margin_bottom=4,
            placeholder_text=_("Add new Sub-Task"),
        )
        entry.connect("activate", self._on_sub_task_added)

        # Uncompleted tasks
        self.uncompleted_task_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
        )

        # Completed tasks
        self.completed_task_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
        )

        # Box
        box: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL, css_classes=["sub-tasks"]
        )
        box.append(entry)
        box.append(self.uncompleted_task_list)
        box.append(self.completed_task_list)
        self.set_child(box)

    def __load_sub_tasks(self) -> None:
        from errands.widgets.task.task import Task

        tasks: list[TaskData] = (
            t
            for t in UserData.get_tasks_as_dicts(self.task.list_uid, self.task.uid)
            if not t.deleted
        )

        for task in tasks:
            new_task = Task(task, self.task.task_list, self.task)
            if task.completed:
                self.completed_task_list.append(new_task)
            else:
                self.uncompleted_task_list.append(new_task)

        self.set_reveal_child(self.task.task_data.expanded)

    # ------ PROPERTIES ------ #

    @property
    def all_tasks(self) -> list[Task]:
        """All tasks in the list"""

        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.tasks)

        __add_task(self.tasks)
        return all_tasks

    @property
    def tasks(self) -> list[Task]:
        """Top-level Tasks"""

        return self.uncompleted_tasks + self.completed_tasks

    @property
    def uncompleted_tasks(self) -> list[Task]:
        return get_children(self.uncompleted_task_list)

    @property
    def completed_tasks(self) -> list[Task]:
        return get_children(self.completed_task_list)

    # ------ PUBLIC METHODS ------ #

    def update_ui(self, update_tasks: bool = True) -> None:
        # Expand
        self.set_reveal_child(self.task.task_data.expanded)

        # Tasks
        sub_tasks_data: list[TaskData] = UserData.get_tasks_as_dicts(
            self.task.list_uid, self.task.uid
        )
        sub_tasks_data_uids: list[str] = [task.uid for task in sub_tasks_data]
        sub_tasks_widgets: list[Task] = self.tasks
        sub_tasks_widgets_uids: list[str] = [task.uid for task in sub_tasks_widgets]

        # Add sub tasks
        for task in sub_tasks_data:
            if task.uid not in sub_tasks_widgets_uids:
                self.task.add_task(task)

        for task in self.tasks:
            # Remove task
            if task.uid not in sub_tasks_data_uids:
                task.purge()
            # Move task to completed tasks
            elif task.task_data.completed and task in self.uncompleted_tasks:
                if (
                    len(self.uncompleted_tasks) > 1
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.task.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.uncompleted_task_list.remove(task)
                self.completed_task_list.prepend(task)
            # Move task to uncompleted tasks
            elif not task.task_data.completed and task in self.completed_tasks:
                if (
                    len(self.uncompleted_tasks) > 0
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.task.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.completed_task_list.remove(task)
                self.uncompleted_task_list.append(task)

        # Update tasks
        if update_tasks:
            for task in self.tasks:
                task.update_ui()

    # ------ SIGNAL HANDLERS ------ #

    def _on_sub_task_added(self, entry: Gtk.Entry) -> None:
        text: str = entry.get_text().strip()

        # Return if entry is empty
        if text.strip(" \n\t") == "":
            return

        # Add sub-task
        self.task.add_task(
            UserData.add_task(
                list_uid=self.task.list_uid,
                text=text,
                parent=self.task.uid,
            )
        )

        # Clear entry
        entry.set_text("")

        # Update status
        if self.task.task_data.completed:
            self.task.update_props(["completed", "synced"], [False, False])

        self.update_ui(False)

        # Sync
        Sync.sync()
