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

from gi.repository import Gtk, Adw, Gdk, GObject, Gio, GLib
from window import Window
from .sub_task import SubTask
from .utils import Animate, Log, Markup, TaskUtils, UserData


@Gtk.Template(resource_path="/io/github/mrvladus/List/task.ui")
class Task(Gtk.Revealer):
    __gtype_name__ = "Task"

    # Template items
    main_box = Gtk.Template.Child()
    task_box_rev = Gtk.Template.Child()
    task_text = Gtk.Template.Child()
    task_status = Gtk.Template.Child()
    expand_icon = Gtk.Template.Child()
    task_completed_btn = Gtk.Template.Child()
    task_edit_box_rev = Gtk.Template.Child()
    task_edit_entry = Gtk.Template.Child()
    sub_tasks_revealer = Gtk.Template.Child()
    delete_completed_btn_revealer = Gtk.Template.Child()
    sub_tasks = Gtk.Template.Child()
    expand_icon = Gtk.Template.Child()

    # State
    expanded: bool = None

    def __init__(self, task: dict, window: Adw.ApplicationWindow):
        super().__init__()
        Log.info("Add task: " + task["text"])
        self.window: Window = window
        self.parent: Gtk.Box = self.window.tasks_list
        self.task: dict = task
        self.add_actions()
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if task completed and toggle checkbox
        if self.task["completed"]:
            self.task_completed_btn.props.active = True
        # Set text
        self.task_text.props.label = self.text
        # Set accent color
        if self.task["color"] != "":
            self.main_box.add_css_class(f'task-{self.task["color"]}')
            self.task_status.add_css_class(f'progress-{self.task["color"]}')
        self.add_sub_tasks()

    def add_actions(self):
        group = Gio.SimpleActionGroup.new()
        self.insert_action_group("task", group)

        def add_action(name: str, callback):
            action = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        add_action("delete", self.delete)
        add_action("edit", self.edit)
        add_action("copy", self.copy)

    def add_sub_tasks(self) -> None:
        tasks: list[dict] = UserData.get()["tasks"]
        sub_count: int = 0
        for task in tasks:
            if task["parent"] == self.task["id"]:
                sub_task = SubTask(task, self, self.window)
                self.sub_tasks.append(sub_task)
                if not task["deleted"]:
                    sub_task.toggle_visibility()
                else:
                    sub_count += 1
        self.expand(sub_count > 0)
        self.update_statusbar()
        self.window.update_status()

    def copy(self, *_) -> None:
        Log.info("Copy to clipboard: " + self.task["text"])
        clp: Gdk.Clipboard = Gdk.Display.get_default().get_clipboard()
        clp.set(self.task["text"])
        self.window.add_toast(self.window.toast_copied)

    def delete(self, *_, update_sts: bool = True) -> None:
        Log.info(f"Delete task: {self.task['text']}")

        self.toggle_visibility()

        data: dict = UserData.get()
        for task in data["tasks"]:
            if task["id"] == self.task["id"]:
                task["deleted"] = self.task["deleted"] = True
                break
        UserData.set(data)

        # Don't update if called externally
        if update_sts:
            self.window.update_status()

        self.window.trash_add(self.task)

    def edit(self, *_) -> None:
        self.toggle_edit_mode()
        # Set entry text and select it
        self.task_edit_entry.get_buffer().props.text = self.task["text"]
        self.task_edit_entry.select_region(0, len(self.task["text"]))
        self.task_edit_entry.grab_focus()

    def expand(self, expanded: bool) -> None:
        self.expanded = expanded
        self.sub_tasks_revealer.set_reveal_child(expanded)
        if expanded:
            self.expand_icon.add_css_class("rotate")
        else:
            self.expand_icon.remove_css_class("rotate")
        self.update_statusbar()

    def toggle_edit_mode(self) -> None:
        self.task_box_rev.set_reveal_child(not self.task_box_rev.get_child_revealed())
        self.task_edit_box_rev.set_reveal_child(
            not self.task_edit_box_rev.get_child_revealed()
        )

    def toggle_visibility(self) -> None:
        self.set_reveal_child(not self.get_child_revealed())

    def update_statusbar(self) -> None:
        n_completed = 0
        n_total = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == self.task["id"]:
                if not task["deleted"]:
                    n_total += 1
                    if task["completed"]:
                        n_completed += 1

        Animate.property(
            self.task_status,
            "fraction",
            self.task_status.props.fraction,
            n_completed / n_total if n_total > 0 else 0,
            250,
        )

        if self.expanded:
            self.task_status.props.visible = True
            self.task_status.add_css_class("task-progressbar")
        else:
            self.task_status.remove_css_class("task-progressbar")
            if n_completed == 0:
                self.task_status.props.visible = False

        # Show delete completed button
        self.delete_completed_btn_revealer.set_reveal_child(n_completed > 0)

    def update_data(self) -> None:
        """Sync self.task with user data.json"""

        data: dict = UserData.get()
        for i, task in enumerate(data["tasks"]):
            if self.task["id"] == task["id"]:
                data["tasks"][i] = self.task
                UserData.set(data)
                return

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_delete_completed_btn_clicked(self, _) -> None:
        history: list = UserData.get()["history"]
        sub_tasks = self.sub_tasks.observe_children()
        for i in range(sub_tasks.get_n_items()):
            sub: SubTask = sub_tasks.get_item(i)
            if sub.task["completed"] and sub.task["id"] not in history:
                sub.delete(update_sts=False)
        self.update_statusbar()

    @Gtk.Template.Callback()
    def on_task_completed_btn_toggled(self, btn: Gtk.Button) -> None:
        self.task["completed"] = btn.props.active
        if btn.props.active:
            self.text = Markup.add_crossline(self.text)
        else:
            self.text = Markup.rm_crossline(self.text)
        self.task_text.props.label = self.text
        self.update_data()
        self.window.update_status()

    @Gtk.Template.Callback()
    def on_expand(self, *_) -> None:
        """Expand task row"""
        self.expand(not self.sub_tasks_revealer.get_child_revealed())

    @Gtk.Template.Callback()
    def on_sub_task_added(self, entry: Gtk.Entry) -> None:
        # Return if entry is empty
        if entry.get_buffer().props.text == "":
            return
        # Add new sub-task
        new_sub_task = TaskUtils.new_sub_task(entry.get_buffer().props.text)
        self.task["sub"].append(new_sub_task)
        self.update_data()
        # Add row
        sub_task = SubTask(new_sub_task, self, self.window)
        self.sub_tasks.append(sub_task)
        sub_task.toggle_visibility()
        self.update_statusbar()
        # Clear entry
        entry.get_buffer().props.text = ""

    @Gtk.Template.Callback()
    def on_task_cancel_edit_btn_clicked(self, *_) -> None:
        self.toggle_edit_mode()

    @Gtk.Template.Callback()
    def on_task_edit(self, entry: Gtk.Entry) -> None:
        old_text: str = self.task["text"]
        new_text: str = entry.get_buffer().props.text
        # Return if text the same or empty
        if new_text == old_text or new_text == "":
            return
        # Change task
        print(f"Change '{old_text}' to '{new_text}'")
        # Set new text
        self.task["text"] = new_text
        self.task["completed"] = False
        self.update_data()
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Toggle checkbox
        self.task_completed_btn.props.active = False
        # Set text
        self.task_text.props.label = self.text
        self.toggle_edit_mode()

    @Gtk.Template.Callback()
    def on_style_selected(self, btn: Gtk.Button) -> None:
        """Apply accent color"""
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
        # Color statusbar
        for c in self.task_status.get_css_classes():
            if "progress-" in c:
                self.task_status.remove_css_class(c)
        if color != "":
            self.task_status.add_css_class(f"progress-{color}")
        # Set new color
        self.task["color"] = color
        self.update_data()

    # --- Drag and Drop --- #

    @Gtk.Template.Callback()
    def on_drag_begin(self, _source, drag) -> bool:
        icon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(Gtk.Button(label=self.task["text"]))

    @Gtk.Template.Callback()
    def on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        value = GObject.Value(Task)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    @Gtk.Template.Callback()
    def on_drop(self, drop, task, _x, _y) -> None:
        # If drop is task
        if task.__gtype_name__ == "Task":
            if task == self or self.get_prev_sibling() == task:
                return

            data = UserData.get()
            tasks = data["tasks"]
            tasks.insert(tasks.index(self.task), tasks.pop(tasks.index(task.task)))
            UserData.set(data)
            self.parent.reorder_child_after(task, self)
            self.parent.reorder_child_after(self, task)

        # If drop is sub-task
        elif task.__gtype_name__ == "SubTask":
            if task.parent == self:
                return

            def check_visible():
                if task.get_child_revealed():
                    return True
                else:
                    task.parent.sub_tasks.remove(task)
                    task.parent.update_data()
                    task.parent.update_statusbar()
                    return False

            task.toggle_visibility()
            GLib.timeout_add(100, check_visible)
            # Remove sub-task
            new_sub_task = task.parent.task["sub"].pop(
                task.parent.task["sub"].index(task.task)
            )
            # Add sub-task
            self.task["sub"].append(new_sub_task)
            sub_task = SubTask(new_sub_task, self, self.window)
            self.sub_tasks.append(sub_task)
            sub_task.toggle_visibility()
            self.update_data()
            self.update_statusbar()
            # Expand
            self.expand(True)

        return True
