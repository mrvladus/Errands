# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.task.task import Task
from errands.widgets.task_list import TaskListContent, TaskListEntry, TaskListHeaderBar

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskListSidebarRow


class TaskList(Adw.Bin):
    def __init__(self, sidebar_row: TaskListSidebarRow) -> None:
        super().__init__()
        self.list_uid: str = sidebar_row.uid
        self.sidebar_row: TaskListSidebarRow = sidebar_row
        self.__build_ui()
        self.__load_tasks()

    # ------ PRIVATE METHODS ------ #

    def __repr__(self) -> str:
        return f"<class 'TaskList' {self.list_uid}>"

    def __build_ui(self) -> None:
        self.content = TaskListContent(self)
        self.entry = TaskListEntry(self)
        self.header_bar = TaskListHeaderBar(self)
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(content=self.content)
        toolbar_view.add_top_bar(self.header_bar)
        toolbar_view.add_top_bar(self.entry)
        self.set_child(toolbar_view)

    def __load_tasks(self) -> None:
        Log.info(f"Task List {self.list_uid}: Load Tasks")

        tasks: list[TaskData] = [
            t for t in UserData.get_tasks_as_dicts(self.list_uid, "") if not t.deleted
        ]
        for task in tasks:
            new_task = Task(task, self, self)
            if task.completed:
                self.content.completed_task_list.append(new_task)
            else:
                self.content.uncompleted_task_list.append(new_task)

        self.header_bar.toggle_completed_btn.set_active(
            UserData.get_list_prop(self.list_uid, "show_completed")
        )

    # ------ PROPERTIES ------ #

    @property
    def tasks(self) -> list[Task]:
        """Top-level Tasks"""

        return self.uncompleted_tasks + self.completed_tasks

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
    def uncompleted_tasks(self) -> list[Task]:
        return get_children(self.content.uncompleted_task_list)

    @property
    def completed_tasks(self) -> list[Task]:
        return get_children(self.content.completed_task_list)

    # ------ PUBLIC METHODS ------ #

    def add_task(self, task: TaskData) -> Task:
        Log.info(f"Task List: Add task '{task.uid}'")

        on_top: bool = GSettings.get("task-list-new-task-position-top")
        new_task = Task(task, self, self)
        if not task.completed:
            if on_top:
                self.content.uncompleted_task_list.prepend(new_task)
            else:
                self.content.uncompleted_task_list.append(new_task)
        else:
            if on_top:
                self.content.completed_task_list.prepend(new_task)
            else:
                self.content.completed_task_list.append(new_task)
        new_task.update_ui()

        return new_task

    def purge(self) -> None:
        State.sidebar.list_box.select_row(self.sidebar_row.get_prev_sibling())
        State.sidebar.list_box.remove(self.sidebar_row)
        State.view_stack.remove(self)
        self.sidebar_row.run_dispose()
        self.run_dispose()

    def update_ui(self, update_tasks: bool = True) -> None:
        self.header_bar.update_ui()
        self.content.update_ui(update_tasks)
