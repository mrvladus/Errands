# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.animation import scroll
from errands.utils.gsettings import GSettings
import errands.utils.tasks as TaskUtils
from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from errands.widgets.trash_item import TrashItem
from gi.repository import Adw, Gtk, GObject, Gio, GLib, Gdk
from errands.widgets.task import Task
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from errands.utils.logging import Log
from errands.utils.tasks import task_to_ics


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/tasks_list.ui")
class TasksList(Adw.Bin):
    __gtype_name__ = "TasksList"

    # Set props
    window = GObject.Property(type=Adw.ApplicationWindow)

    # Template children
    drop_motion_ctrl: Gtk.DropControllerMotion = Gtk.Template.Child()
    scrolled_window: Gtk.ScrolledWindow = Gtk.Template.Child()
    tasks_list: Gtk.Box = Gtk.Template.Child()

    def __init__(self):
        super().__init__()

    def add_task(self, task: dict) -> None:
        new_task = Task(task, self.window)
        self.tasks_list.append(new_task)
        if not task["deleted"]:
            new_task.toggle_visibility(True)

    def get_all_tasks(self) -> list[Task]:
        """
        Get list of all tasks widgets including sub-tasks
        """

        tasks: list[Task] = []

        def append_tasks(items: list[Task]) -> None:
            for task in items:
                tasks.append(task)
                children: list[Task] = get_children(task.tasks_list)
                if len(children) > 0:
                    append_tasks(children)

        append_tasks(get_children(self.tasks_list))
        return tasks

    def get_toplevel_tasks(self) -> list[Task]:
        return get_children(self.tasks_list)

    def load_tasks(self) -> None:
        Log.debug("Loading tasks")

        for task in UserData.get()["tasks"]:
            if not task["parent"]:
                self.add_task(task)
        self.window.update_status()
        # Expand tasks if needed
        if GSettings.get("expand-on-startup"):
            for task in self.get_all_tasks():
                if len(get_children(task.tasks_list)) > 0:
                    task.expand(True)
        Sync.sync(True)

    @Gtk.Template.Callback()
    def on_dnd_scroll(self, _motion, _x, y) -> bool:
        """
        Autoscroll while dragging task
        """

        def _auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.drop_motion_ctrl.contains_pointer():
                return False
            adj = self.scrolled_window.get_vadjustment()
            if scroll_up:
                adj.set_value(adj.get_value() - 2)
                return True
            else:
                adj.set_value(adj.get_value() + 2)
                return True

        MARGIN: int = 50
        height: int = self.scrolled_window.get_allocation().height
        if y < MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, _auto_scroll, True)
        elif y > height - MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, _auto_scroll, False)
        else:
            self.scrolling = False

    @Gtk.Template.Callback()
    def on_scroll(self, adj) -> None:
        """
        Show scroll up button
        """

        self.window.scroll_up_btn_rev.set_reveal_child(adj.get_value() > 0)

    @Gtk.Template.Callback()
    def on_task_added(self, entry: Gtk.Entry) -> None:
        """
        Add new task
        """

        text: str = entry.props.text
        # Check for empty string or task exists
        if text.strip(" \n\t") == "":
            return
        # Add new task
        new_data: UserDataDict = UserData.get()
        new_task: UserDataTask = TaskUtils.new_task(text)
        new_data["tasks"].append(new_task)
        UserData.set(new_data)
        self.add_task(new_task)
        # Clear entry
        entry.props.text = ""
        # Scroll to the end
        scroll(self.scrolled_window, True)
        # Sync
        Sync.sync()

    @Gtk.Template.Callback()
    def on_scroll_up_btn_clicked(self, _) -> None:
        scroll(self.scrolled_window, False)
