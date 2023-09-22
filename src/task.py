# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from gi.repository import Gtk, Adw, Gdk, GObject, Gio
from .sync import Sync
from .utils import GSettings, Log, Markup, TaskUtils, UserData, get_children


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/task.ui")
class Task(Gtk.Revealer):
    __gtype_name__ = "Task"

    # - Template children - #
    main_box: Gtk.Box = Gtk.Template.Child()
    task_box_rev: Gtk.Revealer = Gtk.Template.Child()
    task_row: Adw.ActionRow = Gtk.Template.Child()
    expand_icon: Gtk.Image = Gtk.Template.Child()
    task_completed_btn: Gtk.Button = Gtk.Template.Child()
    task_edit_entry: Gtk.Entry = Gtk.Template.Child()
    sub_tasks_revealer: Gtk.Revealer = Gtk.Template.Child()
    tasks_list: Gtk.Box = Gtk.Template.Child()

    # - State - #
    expanded: bool = False
    is_sub_task: bool = False

    def __init__(self, task: dict, window: Adw.ApplicationWindow, parent=None) -> None:
        super().__init__()
        Log.info(f"Add {'task' if not task['parent'] else 'sub-task'}: " + task["id"])
        self.window = window
        self.parent = self.window if not parent else parent
        self.task: dict = task
        # Set text
        self.text = Markup.find_url(Markup.escape(self.task["text"]))
        self.task_row.props.title = self.text
        # Check if sub-task completed and toggle checkbox
        self.task_completed_btn.props.active = self.task["completed"]
        # Set accent color
        if self.task["color"] != "":
            self.main_box.add_css_class(f'task-{self.task["color"]}')
        # Add to trash if needed
        if self.task["deleted"]:
            self.window.trash_add(self.task)
        # Add to main task list
        self.window.tasks.append(self)
        self.check_is_sub()
        self.add_sub_tasks()
        self.add_actions()

    def __repr__(self):
        return f"<Task> {self.task['id']}"

    def add_actions(self) -> None:
        group = Gio.SimpleActionGroup.new()
        self.insert_action_group("task", group)

        def add_action(name: str, callback):
            action = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        def copy(*_) -> None:
            Log.info("Copy to clipboard: " + self.task["text"])
            clp: Gdk.Clipboard = Gdk.Display.get_default().get_clipboard()
            clp.set(self.task["text"])
            self.window.add_toast(self.window.toast_copied)

        def edit(*_) -> None:
            self.toggle_edit_mode()
            # Set entry text and select it
            self.task_edit_entry.get_buffer().props.text = self.task["text"]
            self.task_edit_entry.select_region(0, len(self.task["text"]))
            self.task_edit_entry.grab_focus()

        add_action("delete", self.delete)
        add_action("edit", edit)
        add_action("copy", copy)

    def add_sub_task(self, task: dict):
        sub_task = Task(task, self.window, self)
        self.tasks_list.append(sub_task)
        sub_task.toggle_visibility(not task["deleted"])

    def add_sub_tasks(self) -> None:
        sub_count = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == self.task["id"]:
                sub_count += 1
                self.add_sub_task(task)
        self.expand(sub_count > 0 and GSettings.get("expand-on-startup"))
        self.update_status()
        self.window.update_status()

    def check_is_sub(self):
        if self.task["parent"]:
            self.is_sub_task = True
            self.main_box.add_css_class("sub-task")
        else:
            self.main_box.add_css_class("task")

    def delete(self, *_) -> None:
        Log.info(f"Delete task: {self.task['id']}")

        self.toggle_visibility(False)
        self.task["deleted"] = True
        self.update_data()
        if self.is_sub_task:
            self.parent.update_status()
        self.window.update_status()
        self.window.trash_add(self.task)

    def expand(self, expanded: bool) -> None:
        self.expanded = expanded
        self.sub_tasks_revealer.set_reveal_child(expanded)
        if expanded:
            self.expand_icon.add_css_class("rotate")
        else:
            self.expand_icon.remove_css_class("rotate")
        self.update_status()

    def purge(self):
        """
        Completely remove widget
        """

        self.window.tasks.remove(self)
        self.parent.tasks_list.remove(self)

    def toggle_edit_mode(self) -> None:
        self.task_box_rev.set_reveal_child(not self.task_box_rev.get_child_revealed())

    def toggle_visibility(self, on: bool) -> None:
        self.set_reveal_child(on)

    def update_status(self) -> None:
        n_completed = 0
        n_total = 0
        for task in get_children(self.tasks_list):
            if not task.task["deleted"]:
                n_total += 1
                if task.task["completed"]:
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

        data: dict = UserData.get()
        for i, task in enumerate(data["tasks"]):
            if self.task["id"] == task["id"]:
                data["tasks"][i] = self.task
                UserData.set(data)
                return

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_task_completed_btn_toggled(self, btn: Gtk.Button) -> None:
        """
        Toggle check button and add style to the text
        """

        self.task["completed"] = btn.props.active
        self.update_data()

        for task in get_children(self.tasks_list):
            task.task_completed_btn.set_active(btn.props.active)

        # Update status
        if self.is_sub_task:
            self.parent.update_status()
        self.window.update_status()
        # Set crosslined text
        if btn.props.active:
            self.text = Markup.add_crossline(self.text)
            self.add_css_class("task-completed")
        else:
            self.text = Markup.rm_crossline(self.text)
            self.remove_css_class("task-completed")
        self.task_row.props.title = self.text

    @Gtk.Template.Callback()
    def on_expand(self, *_) -> None:
        """
        Expand task row
        """

        self.expand(not self.sub_tasks_revealer.get_child_revealed())

    @Gtk.Template.Callback()
    def on_sub_task_added(self, entry: Gtk.Entry) -> None:
        """
        Add new Sub-Task
        """

        # Return if entry is empty
        if entry.get_buffer().props.text == "":
            return
        # Add new sub-task
        new_sub_task = TaskUtils.new_task(
            entry.get_buffer().props.text, pid=self.task["id"]
        )
        data: dict = UserData.get()
        data["tasks"].append(new_sub_task)
        UserData.set(data)
        # Add sub-task
        self.add_sub_task(new_sub_task)
        # Clear entry
        entry.get_buffer().props.text = ""
        # Update status
        self.task_completed_btn.props.active = self.task["completed"] = False
        self.update_data()
        self.update_status()
        self.window.update_status()
        Sync.sync()

    @Gtk.Template.Callback()
    def on_task_cancel_edit_btn_clicked(self, *_) -> None:
        self.toggle_edit_mode()

    @Gtk.Template.Callback()
    def on_task_edit(self, entry: Gtk.Entry) -> None:
        """
        Edit task text
        """

        old_text: str = self.task["text"]
        new_text: str = entry.get_buffer().props.text
        # Return if text the same or empty
        if new_text == old_text or new_text == "":
            return
        # Change task
        Log.info(f"Edit: {self.task['id']}")
        # Set new text
        self.task["text"] = new_text
        # Escape text and find URL's'
        self.text = Markup.find_url(Markup.escape(self.task["text"]))
        self.task_row.props.title = self.text
        # Toggle checkbox
        self.task_completed_btn.props.active = self.task["completed"] = False
        self.update_data()
        # Exit edit mode
        self.toggle_edit_mode()
        Sync.sync()

    @Gtk.Template.Callback()
    def on_style_selected(self, btn: Gtk.Button) -> None:
        """
        Apply accent color
        """

        for i in btn.get_css_classes():
            color = ""
            if i.startswith("btn-"):
                color = i.split("-")[1]
                break
        # Color card
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        self.main_box.add_css_class(f"task-{color}")
        # Set new color
        self.task["color"] = color
        self.update_data()

    # --- Drag and Drop --- #

    @Gtk.Template.Callback()
    def on_drag_end(self, *_) -> bool:
        self.set_sensitive(True)

    @Gtk.Template.Callback()
    def on_drag_begin(self, _, drag) -> bool:
        icon = Gtk.DragIcon.get_for_drag(drag)
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
    def on_task_top_drop(self, drop, task, _x, _y) -> bool:
        """
        When task is dropped on "+" area on top of task
        """

        # Return if task is itself
        if task == self:
            return False

        # Move data
        data = UserData.get()
        tasks = data["tasks"]
        tasks.insert(tasks.index(self.task), tasks.pop(tasks.index(task.task)))
        UserData.set(data)

        # If task has the same parent
        if task.parent == self.parent:
            # Move widget
            self.parent.tasks_list.reorder_child_after(task, self)
            self.parent.tasks_list.reorder_child_after(self, task)
            return True

        # Change parent if different parents
        task.task["parent"] = self.task["parent"]
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

        return True

    @Gtk.Template.Callback()
    def on_drop(self, drop, task, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """

        if task == self or task.parent == self:
            return

        # Change parent
        task.task["parent"] = self.task["id"]
        task.update_data()
        # Move data
        data = UserData.get()
        tasks = data["tasks"]
        last_sub_idx: int = 0
        for t in tasks:
            if t["parent"] == self.task["id"]:
                last_sub_idx = tasks.index(t)
        tasks.insert(
            tasks.index(self.task) + last_sub_idx, tasks.pop(tasks.index(task.task))
        )
        UserData.set(data)
        # Remove old task
        task.purge()
        # Add new sub-task
        self.expand(True)
        sub_task = Task(task.task.copy(), self.window, self)
        self.tasks_list.append(sub_task)
        sub_task.toggle_visibility(True)
        # Update status
        task.parent.update_status()
        self.update_status()

        return True
