# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.animation import scroll
from errands.utils.gsettings import GSettings
import errands.utils.tasks as TaskUtils
from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from errands.widgets.trash import TrashPanel
from gi.repository import Adw, Gtk, GObject, GLib
from errands.widgets.task import Task
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from errands.utils.logging import Log


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/tasks_list.ui")
class TasksList(Adw.Bin):
    __gtype_name__ = "TasksList"

    # Set props
    window = GObject.Property(type=Adw.ApplicationWindow)
    trash_panel = GObject.Property(type=TrashPanel)

    # Template children
    drop_motion_ctrl: Gtk.DropControllerMotion = Gtk.Template.Child()
    scrolled_window: Gtk.ScrolledWindow = Gtk.Template.Child()
    tasks_list: Gtk.Box = Gtk.Template.Child()
    delete_completed_tasks_btn = Gtk.Template.Child()
    title: Adw.WindowTitle = Gtk.Template.Child()
    scroll_up_btn_rev = Gtk.Template.Child()
    sync_btn: Gtk.Button = Gtk.Template.Child()
    split_view = Gtk.Template.Child()
    task_details = Gtk.Template.Child()
    trash_panel = Gtk.Template.Child()
    stack = Gtk.Template.Child()

    # State
    scrolling: bool = False  # Is window scrolling
    startup: bool = True

    def __init__(self):
        super().__init__()
        self.trash_panel.tasks_list = self

    def add_task(self, task: dict) -> None:
        new_task = Task(task, self)
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
        self.update_status()
        # Expand tasks if needed
        if GSettings.get("expand-on-startup"):
            for task in self.get_all_tasks():
                if len(get_children(task.tasks_list)) > 0:
                    task.expand(True)
        Sync.sync(True)

    def update_status(self) -> None:
        """
        Update status bar on the top
        """

        tasks: list[UserDataTask] = UserData.get()["tasks"]
        n_total: int = 0
        n_completed: int = 0
        n_all_deleted: int = 0
        n_all_completed: int = 0

        for task in tasks:
            if task["parent"] == "":
                if not task["deleted"]:
                    n_total += 1
                    if task["completed"]:
                        n_completed += 1
            if not task["deleted"]:
                if task["completed"]:
                    n_all_completed += 1
            else:
                n_all_deleted += 1

        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}"  # pyright: ignore
            if n_total > 0
            else ""
        )
        self.delete_completed_tasks_btn.set_reveal_child(n_all_completed > 0)
        self.trash_panel.trash_list_scrl.set_visible(n_all_deleted > 0)

    def update_ui(self) -> None:
        Log.debug("Updating UI")

        # Update existing tasks
        tasks: list[Task] = self.get_all_tasks()
        data_tasks: list[UserDataTask] = UserData.get()["tasks"]
        to_change_parent: list[UserDataTask] = []
        to_remove: list[Task] = []
        for task in tasks:
            for t in data_tasks:
                if task.task["id"] == t["id"]:
                    # If parent is changed
                    if task.task["parent"] != t["parent"]:
                        to_change_parent.append(t)
                        to_remove.append(task)
                        break
                    # If text changed
                    if task.task["text"] != t["text"]:
                        task.task["text"] = t["text"]
                        task.text = Markup.find_url(Markup.escape(task.task["text"]))
                        task.task_row.props.title = task.text
                    # If completion changed
                    if task.task["completed"] != t["completed"]:
                        task.completed_btn.props.active = t["completed"]

        # Remove old tasks
        for task in to_remove:
            task.purge()

        # Change parents
        for task in to_change_parent:
            if task["parent"] == "":
                self.tasks_list.add_task(task)
            else:
                for t in tasks:
                    if t.task["id"] == task["parent"]:
                        t.add_task(task)
                        break

        # Create new tasks
        tasks_ids: list[str] = [
            task.task["id"] for task in self.tasks_list.get_all_tasks()
        ]
        for task in data_tasks:
            if task["id"] not in tasks_ids:
                # Add toplevel task and its sub-tasks
                if task["parent"] == "":
                    self.tasks_list.add_task(task)
                # Add sub-task and its sub-tasks
                else:
                    for t in self.tasks_list.get_all_tasks():
                        if t.task["id"] == task["parent"]:
                            t.add_task(task)
                tasks_ids = [
                    task.task["id"] for task in self.tasks_list.get_all_tasks()
                ]

        # Remove tasks
        ids = [t["id"] for t in UserData.get()["tasks"]]
        for task in self.tasks_list.get_all_tasks():
            if task.task["id"] not in ids:
                task.purge()

    @Gtk.Template.Callback()
    def on_delete_completed_tasks_btn_clicked(self, _) -> None:
        """
        Hide completed tasks and move them to trash
        """
        Log.info("Delete completed tasks")

        for task in self.get_all_tasks():
            if task.task["completed"] and not task.task["deleted"]:
                task.delete()
        self.update_status()

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

        self.scroll_up_btn_rev.set_reveal_child(adj.get_value() > 0)

    @Gtk.Template.Callback()
    def on_scroll_up_btn_clicked(self, _) -> None:
        """
        Scroll up
        """

        scroll(self.tasks_list.scrolled_window, False)

    @Gtk.Template.Callback()
    def on_sync_btn_clicked(self, btn) -> None:
        Sync.sync(True)

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
