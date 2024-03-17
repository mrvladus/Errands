# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
import os
from typing import TYPE_CHECKING


if TYPE_CHECKING:
    from errands.widgets.window import Window
    from errands.widgets.sidebar.task_list_row import TaskListRow

from errands.lib.sync.sync import Sync
from gi.repository import Adw, Gtk, GLib, Gio, GObject  # type:ignore
from errands.lib.animation import scroll
from errands.lib.data import TaskData, UserData
from errands.lib.utils import get_children, timeit
from errands.lib.logging import Log
from errands.widgets.task.task import Task
from errands.lib.gsettings import GSettings


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TaskList(Adw.Bin):
    __gtype_name__ = "TaskList"

    title: Adw.WindowTitle = Gtk.Template.Child()
    delete_completed_btn: Gtk.Button = Gtk.Template.Child()
    toggle_completed_btn: Gtk.ToggleButton = Gtk.Template.Child()
    scroll_up_btn: Gtk.Button = Gtk.Template.Child()
    scrl: Gtk.ScrolledWindow = Gtk.Template.Child()
    uncompleted_tasks_list: Gtk.Box = Gtk.Template.Child()
    completed_tasks_list: Gtk.Box = Gtk.Template.Child()

    # State
    scrolling: bool = False

    def __init__(self, list_uid: str, sidebar_row) -> None:
        super().__init__()
        self.window: Window = Adw.Application.get_default().get_active_window()
        self.list_uid: str = list_uid
        self.sidebar_row: TaskListRow = sidebar_row
        self.__load_tasks()

    def __repr__(self) -> str:
        return f"<class 'TaskList' {self.list_uid}>"

    # ------ PRIVATE METHODS ------ #

    def __load_tasks(self) -> None:
        tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if not t["deleted"] and t["parent"] == ""
        ]
        for task in tasks:
            new_task = Task(task["uid"], self, self)
            if task["completed"]:
                self.completed_tasks_list.append(new_task)
            else:
                self.uncompleted_tasks_list.append(new_task)
            new_task.update_ui()

    def __sort_tasks(self) -> None:
        pass

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
        return get_children(self.uncompleted_tasks_list)

    @property
    def completed_tasks(self) -> list[Task]:
        return get_children(self.completed_tasks_list)

    # ------ PUBLIC METHODS ------ #

    def add_task(self, uid: str) -> Task:
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        new_task = Task(uid, self, self)
        task_list = (
            self.completed_tasks_list
            if new_task.get_prop("completed")
            else self.completed_tasks_list
        )
        if on_top:
            task_list.prepend(new_task)
        else:
            task_list.append(new_task)
        new_task.update_ui()

        return new_task

    def update_ui(self, update_tasks_ui: bool = True, sort: bool = True) -> None:
        Log.debug(f"Task list {self.list_uid}: Update UI")

        # Update tasks
        data_uids: list[str] = [
            t["uid"]
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if t["parent"] == "" and not t["deleted"]
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for uid in data_uids:
            if uid not in widgets_uids:
                self.add_task(uid)

        for task in self.tasks:
            # Remove task
            if task.uid not in data_uids:
                task.purge()
            # Move task to completed tasks
            elif task.get_prop("completed") and task in self.uncompleted_tasks:
                if (
                    len(self.uncompleted_tasks) > 1
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.uncompleted_tasks_list.remove(task)
                self.completed_tasks_list.prepend(task)
            # Move task to uncompleted tasks
            elif not task.get_prop("completed") and task in self.completed_tasks:
                if (
                    len(self.uncompleted_tasks) > 0
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.completed_tasks_list.remove(task)
                self.uncompleted_tasks_list.append(task)

        # Update tasks
        if update_tasks_ui:
            for task in self.tasks:
                task.update_ui()

        # Sort tasks
        self.__sort_tasks()

        # Update status
        tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if not t["trash"] and not t["deleted"]
        ]
        n_total: int = len(tasks)
        n_completed: int = len([t for t in tasks if t["completed"]])
        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}" if n_total > 0 else ""
        )

        # Update sidebar item counter
        self.sidebar_row.size_counter.set_label(str(n_total) if n_total > 0 else "")

        # Update delete completed button
        self.delete_completed_btn.set_sensitive(n_completed > 0)

        # Rename list
        self.title.set_title(
            UserData.run_sql(
                f"""SELECT name FROM lists
                WHERE uid = '{self.list_uid}'""",
                fetch=True,
            )[0][0]
        )

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_delete_completed_btn_clicked(self, _) -> None:
        """Hide completed tasks and move them to trash"""

        Log.info("Delete completed tasks")
        for task in self.all_tasks:
            if not task.get_prop("trash") and task.get_prop("completed"):
                task.delete()
        self.window.sidebar.update_ui()

    @Gtk.Template.Callback()
    def _on_toggle_completed_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        """Switch visibility of completed tasks"""

        Log.info(
            f"Task List: Toggle completed tasks '{'off' if btn.get_active() else 'on'}'"
        )
        for task in self.all_tasks:
            task.update_ui()

    @Gtk.Template.Callback()
    def _on_dnd_scroll(self, _motion, _x, y: float) -> bool:
        """Autoscroll while dragging task"""
        return

        def __auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.dnd_ctrl.contains_pointer():
                return False
            self.adj.set_value(self.adj.get_value() - (2 if scroll_up else -2))
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

    @Gtk.Template.Callback()
    def _on_scroll_up_btn_clicked(self, _) -> None:
        scroll(self.scrl, False)

    @Gtk.Template.Callback()
    def _on_scroll(self, adj) -> None:
        self.scroll_up_btn.set_visible(adj.get_value() > 0)

    @Gtk.Template.Callback()
    def _on_task_added(self, entry: Adw.EntryRow) -> None:
        text: str = entry.get_text()
        if text.strip(" \n\t") == "":
            return
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        self.add_task(
            UserData.add_task(
                list_uid=self.list_uid,
                text=text,
                insert_at_the_top=on_top,
            )
        )
        entry.set_text("")
        if not on_top:
            scroll(self.scrl, True)
        self.update_ui(False, False)
        Sync.sync(False)
