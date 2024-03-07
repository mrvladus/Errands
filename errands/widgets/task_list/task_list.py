# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
import os
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.sync.sync import Sync
from gi.repository import Adw, Gtk, GLib, GObject  # type:ignore
from errands.lib.animation import scroll
from errands.lib.data import TaskData, UserData
from errands.lib.utils import get_children
from errands.lib.logging import Log
from errands.widgets.task.task import Task
from errands.lib.gsettings import GSettings


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TaskList(Adw.Bin):
    __gtype_name__ = "TaskList"

    title: Adw.WindowTitle = Gtk.Template.Child()
    delete_completed_btn: Gtk.Button = Gtk.Template.Child()
    scroll_up_btn: Gtk.Button = Gtk.Template.Child()
    scrl: Gtk.ScrolledWindow = Gtk.Template.Child()
    task_list: Gtk.ListBox = Gtk.Template.Child()

    # State
    scrolling: bool = False

    def __init__(self, window: Window, list_uid: str):
        super().__init__()
        self.window: Window = window
        self.list_uid: str = list_uid

    @property
    def tasks(self) -> list[Task]:
        return get_children(self.task_list)

    @property
    def all_tasks(self) -> list[Task]:
        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.tasks)

        __add_task(self.tasks)

        return all_tasks

    def update_ui(self) -> None:
        Log.debug(f"Task list {self.list_uid}: Update UI")

        # Rename list
        self.title.set_title(
            UserData.run_sql(
                f"""SELECT name FROM lists 
                WHERE uid = '{self.list_uid}'""",
                fetch=True,
            )[0][0]
        )

        # Update tasks
        data_uids: list[str] = [
            t["uid"]
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if t["parent"] == "" and not t["completed"] and not t["deleted"]
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        for uid in data_uids:
            if uid not in widgets_uids:
                new_task = Task(uid, self, self.task_list, False)
                if on_top:
                    self.task_list.prepend(new_task)
                else:
                    self.task_list.append(new_task)

        # Remove tasks
        for task in self.tasks:
            if task.uid not in data_uids:
                task.purged = True

        # Update tasks
        for task in self.tasks:
            task.update_ui()

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

        # Update delete completed button
        self.delete_completed_btn.set_sensitive(n_completed > 0)

    @Gtk.Template.Callback()
    def _on_delete_completed_btn_clicked(self, _) -> None:
        """Hide completed tasks and move them to trash"""

        Log.info("Delete completed tasks")
        for task in self.all_tasks:
            if not task.get_prop("trash") and task.get_prop("completed"):
                task.delete()
        self.window.sidebar.update_ui()

    @Gtk.Template.Callback()
    def _on_dnd_scroll(self, _motion, _x, y: float) -> bool:
        """Autoscroll while dragging task"""

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
    def _on_scroll_up_btn_clicked(self, _):
        scroll(self.scrl, False)

    @Gtk.Template.Callback()
    def _on_scroll(self, adj):
        self.scroll_up_btn.set_visible(adj.get_value() > 0)

    @Gtk.Template.Callback()
    def _on_task_added(self, entry: Adw.EntryRow) -> None:
        text: str = entry.get_text()
        if text.strip(" \n\t") == "":
            return
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        UserData.add_task(
            list_uid=self.list_uid,
            text=text,
            insert_at_the_top=on_top,
        )
        entry.set_text("")
        if not on_top:
            scroll(self.scrl, True)
        self.update_ui()
        Sync.sync()
