# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import Self
from gi.repository import Gtk, Adw, Gdk, GObject

# Import modules
import errands.utils.tasks as TaskUtils
from errands.utils.sync import Sync
from errands.utils.logging import Log
from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.markup import Markup
from errands.utils.functions import get_children


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/task.ui")
class Task(Gtk.Revealer):
    __gtype_name__ = "Task"

    # - Template children - #
    main_box: Gtk.Box = Gtk.Template.Child()
    task_row: Adw.ActionRow = Gtk.Template.Child()
    expand_icon: Gtk.Image = Gtk.Template.Child()
    completed_btn: Gtk.Button = Gtk.Template.Child()
    sub_tasks_revealer: Gtk.Revealer = Gtk.Template.Child()
    tasks_list: Gtk.Box = Gtk.Template.Child()

    # - State - #
    just_added: bool = True
    is_sub_task: bool = False
    can_sync: bool = True

    def __init__(
        self, task: UserDataTask, window: Adw.ApplicationWindow, parent=None
    ) -> None:
        super().__init__()
        Log.info(f"Add {'task' if not task['parent'] else 'sub-task'}: " + task["id"])
        self.window: Adw.ApplicationWindow = window
        self.parent: Adw.ApplicationWindow | Task = (
            self.window if not parent else parent
        )
        self.task: UserDataTask = task
        # Set text
        self.task_row.set_title(Markup.find_url(Markup.escape(self.task["text"])))
        # Check if sub-task completed and toggle checkbox
        self.completed_btn.props.active = self.task["completed"]
        # Set accent color
        if self.task["color"] != "":
            self.main_box.add_css_class(f'task-{self.task["color"]}')
        # Add to trash if needed
        if self.task["deleted"]:
            self.window.trash_panel.trash_add(self.task)
        self._check_is_sub()
        self._add_sub_tasks()
        self.just_added = False
        self.parent.update_status()

    def __repr__(self) -> str:
        return f"Task({self.task['id']})"

    def add_task(self, task: dict) -> None:
        sub_task: Task = Task(task, self.window, self)
        self.tasks_list.append(sub_task)
        sub_task.toggle_visibility(not task["deleted"])
        if not self.just_added:
            self.update_status()

    def _add_sub_tasks(self) -> None:
        sub_count: int = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == self.task["id"]:
                sub_count += 1
                self.add_task(task)
        self.update_status()
        self.window.update_status()

    def _check_is_sub(self) -> None:
        if self.task["parent"] != "":
            self.is_sub_task = True
            self.main_box.add_css_class("sub-task")
            if not self.window.startup and self.parent != self.window:
                self.parent.expand(True)
        else:
            self.main_box.add_css_class("task")

    def delete(self, *_) -> None:
        Log.info(f"Move task to trash: {self.task['id']}")

        self.toggle_visibility(False)
        self.task["deleted"] = True
        self.update_data()
        self.completed_btn.set_active(True)
        self.window.trash_panel.trash_add(self.task)
        for task in get_children(self.tasks_list):
            if not task.task["deleted"]:
                task.delete()
        if self.window.task_details.parent == self:
            self.window.task_details.details_status.set_visible(True)

    def expand(self, expanded: bool) -> None:
        self.sub_tasks_revealer.set_reveal_child(expanded)
        if expanded:
            self.expand_icon.add_css_class("rotate")
        else:
            self.expand_icon.remove_css_class("rotate")

    def purge(self) -> None:
        """
        Completely remove widget
        """

        self.parent.tasks_list.remove(self)
        self.run_dispose()

    def toggle_visibility(self, on: bool) -> None:
        self.set_reveal_child(on)

    def update_status(self) -> None:
        n_completed: int = 0
        n_total: int = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == self.task["id"]:
                if not task["deleted"]:
                    n_total += 1
                    if task["completed"]:
                        n_completed += 1

        self.task_row.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}"  # pyright: ignore
            if n_total > 0
            else ""
        )

    def update_data(self) -> None:
        """
        Sync self.task with user data.json
        """

        data: UserDataDict = UserData.get()
        for i, task in enumerate(data["tasks"]):
            if self.task["id"] == task["id"]:
                data["tasks"][i] = self.task
                UserData.set(data)
                return

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn: Gtk.Button) -> None:
        """
        Toggle check button and add style to the text
        """

        def _set_text():
            if btn.get_active():
                text = Markup.add_crossline(self.task["text"])
                self.add_css_class("task-completed")
            else:
                text = Markup.rm_crossline(self.task["text"])
                self.remove_css_class("task-completed")
            self.task_row.set_title(text)

        # If task is just added set text and return to avoid useless sync
        if self.just_added:
            _set_text()
            return

        # Update data
        self.task["completed"] = btn.get_active()
        self.task["synced_caldav"] = False
        self.update_data()
        # Update children
        children: list[Task] = get_children(self.tasks_list)
        for task in children:
            task.can_sync = False
            task.completed_btn.set_active(btn.get_active())
        # Update status
        if self.is_sub_task:
            self.parent.update_status()
        # Set text
        _set_text()
        # Sync
        if self.can_sync:
            Sync.sync()
            self.window.update_status()
            for task in children:
                task.can_sync = True

    @Gtk.Template.Callback()
    def on_expand(self, *_) -> None:
        """
        Expand task row
        """

        self.expand(not self.sub_tasks_revealer.get_child_revealed())

    @Gtk.Template.Callback()
    def on_details_btn_clicked(self, _btn):
        self.window.stack.set_visible_child_name("details")
        self.window.task_details.update_info(self)

    @Gtk.Template.Callback()
    def on_sub_task_added(self, entry: Gtk.Entry) -> None:
        """
        Add new Sub-Task
        """

        # Return if entry is empty
        if entry.get_buffer().props.text == "":
            return
        # Add new sub-task
        new_sub_task: UserDataTask = TaskUtils.new_task(
            entry.get_buffer().props.text, parent=self.task["id"]
        )
        data: UserDataDict = UserData.get()
        data["tasks"].append(new_sub_task)
        UserData.set(data)
        # Add sub-task
        self.add_task(new_sub_task)
        # Clear entry
        entry.get_buffer().props.text = ""
        # Update status
        self.task["completed"] = False
        self.update_data()
        self.just_added = True
        self.completed_btn.set_active(False)
        self.just_added = False
        self.update_status()
        self.window.update_status()
        # Sync
        Sync.sync()

    # --- Drag and Drop --- #

    @Gtk.Template.Callback()
    def on_drag_end(self, *_) -> bool:
        self.set_sensitive(True)

    @Gtk.Template.Callback()
    def on_drag_begin(self, _, drag) -> bool:
        icon: Gtk.DragIcon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(
            Gtk.Button(
                label=self.task["text"]
                if len(self.task["text"]) < 20
                else f"{self.task['text'][0:20]}..."
            )
        )

    @Gtk.Template.Callback()
    def on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        self.set_sensitive(False)
        value = GObject.Value(Task)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    @Gtk.Template.Callback()
    def on_task_top_drop(self, _drop, task, _x, _y) -> bool:
        """
        When task is dropped on "+" area on top of task
        """

        # Return if task is itself
        if task == self:
            return False

        # Move data
        data: UserDataDict = UserData.get()
        tasks = data["tasks"]
        for i, t in enumerate(tasks):
            if t["id"] == self.task["id"]:
                self_idx = i
            elif t["id"] == task.task["id"]:
                task_idx = i
        tasks.insert(self_idx, tasks.pop(task_idx))
        UserData.set(data)

        # If task has the same parent
        if task.parent == self.parent:
            # Move widget
            self.parent.tasks_list.reorder_child_after(task, self)
            self.parent.tasks_list.reorder_child_after(self, task)
            return True

        # Change parent if different parents
        task.task["parent"] = self.task["parent"]
        task.task["synced_caldav"] = False
        task.update_data()
        task.purge()
        # Add new task widget
        new_task = Task(task.task, self.window, self.parent)
        self.parent.tasks_list.append(new_task)
        self.parent.tasks_list.reorder_child_after(new_task, self)
        self.parent.tasks_list.reorder_child_after(self, new_task)
        new_task.toggle_visibility(True)
        # Update status
        self.parent.update_status()
        task.parent.update_status()

        # Sync
        Sync.sync()

        return True

    @Gtk.Template.Callback()
    def on_drop(self, _drop, task: Self, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """

        if task == self or task.parent == self:
            return

        # Change parent
        task.task["parent"] = self.task["id"]
        task.task["synced_caldav"] = False
        task.update_data()
        # Move data
        data: UserDataDict = UserData.get()
        tasks = data["tasks"]
        last_sub_idx: int = 0
        for i, t in enumerate(tasks):
            if t["parent"] == self.task["id"]:
                last_sub_idx = tasks.index(t)
            if t["id"] == self.task["id"]:
                self_idx = i
            if t["id"] == task.task["id"]:
                task_idx = i
        tasks.insert(self_idx + last_sub_idx, tasks.pop(task_idx))
        UserData.set(data)
        # Remove old task
        task.purge()
        # Add new sub-task
        self.add_task(task.task.copy())
        self.task["completed"] = False
        self.update_data()
        self.just_added = True
        self.completed_btn.set_active(False)
        self.just_added = False
        # Update status
        task.parent.update_status()
        self.update_status()
        self.parent.update_status()

        # Sync
        Sync.sync()

        return True
