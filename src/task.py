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

from gi.repository import Gtk, Adw, Gdk, GObject
from .sub_task import SubTask
from .utils import Animation, GSettings, Markup, TaskUtils, UserData


@Gtk.Template(resource_path="/io/github/mrvladus/List/task.ui")
class Task(Gtk.Revealer):
    __gtype_name__ = "Task"

    # Template items
    main_box = Gtk.Template.Child()
    task_box_rev = Gtk.Template.Child()
    task_text = Gtk.Template.Child()
    task_status = Gtk.Template.Child()
    expand_btn = Gtk.Template.Child()
    accent_colors_btn = Gtk.Template.Child()
    accent_colors_popover = Gtk.Template.Child()
    task_completed_btn = Gtk.Template.Child()
    task_edit_box_rev = Gtk.Template.Child()
    task_edit_entry = Gtk.Template.Child()
    sub_tasks_revealer = Gtk.Template.Child()
    delete_completed_btn_revealer = Gtk.Template.Child()
    sub_tasks = Gtk.Template.Child()

    # State
    expanded: bool = None

    def __init__(self, task: dict, window: Adw.ApplicationWindow):
        super().__init__()
        print("Add task:", task["text"])
        self.window = window
        self.parent = self.window.tasks_list
        self.task = task
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
        # Expand sub tasks
        self.expand(
            self.task["sub"] != []
            and GSettings.get("tasks-expanded")
            and GSettings.get("enable-sub-tasks")
        )
        # Show or hide accent colors menu
        self.accent_colors_btn.set_visible(GSettings.get("show-accent-colors-menu"))
        self.add_sub_tasks()
        self.update_statusbar()

    def add_sub_tasks(self) -> None:
        # Hide sub-tasks if disabeled
        if not GSettings.get("enable-sub-tasks"):
            self.expand_btn.props.visible = False
            self.task_status.props.visible = False
            return
        if self.task["sub"] == []:
            return
        history: list = UserData.get()["history"]
        for task in self.task["sub"]:
            sub_task = SubTask(task, self, self.window)
            self.sub_tasks.append(sub_task)
            if task["id"] not in history:
                sub_task.toggle_visibility()

    def delete(self) -> None:
        print(f"Delete task: {self.task['text']}")
        self.toggle_visibility()
        new_data: dict = UserData.get()
        new_data["history"].append(self.task["id"])
        UserData.set(new_data)
        self.window.update_undo()

    def expand(self, expanded: bool) -> None:
        self.expanded = expanded
        self.sub_tasks_revealer.set_reveal_child(expanded)
        self.expand_btn.set_icon_name(
            "go-up-symbolic" if expanded else "go-down-symbolic"
        )
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
        for sub in self.task["sub"]:
            if sub["id"] not in UserData.get()["history"]:
                n_total += 1
                if sub["completed"]:
                    n_completed += 1
        if n_total > 0:
            Animation(
                self.task_status,
                "fraction",
                self.task_status.props.fraction,
                n_completed / n_total,
                250,
            )
        else:
            Animation(
                self.task_status,
                "fraction",
                self.task_status.props.fraction,
                0,
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
        new_data: dict = UserData.get()
        for i, task in enumerate(new_data["tasks"]):
            if self.task["id"] == task["id"]:
                new_data["tasks"][i] = self.task
                UserData.set(new_data)
                return

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_task_delete(self, _) -> None:
        self.delete()
        self.window.update_status()

    @Gtk.Template.Callback()
    def on_delete_completed_btn_clicked(self, _) -> None:
        history: list = UserData.get()["history"]
        sub_tasks = self.sub_tasks.observe_children()
        for i in range(sub_tasks.get_n_items()):
            sub = sub_tasks.get_item(i)
            if sub.task["completed"] and sub.task["id"] not in history:
                sub.delete()
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
    def on_expand_btn_clicked(self, _) -> None:
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
    def on_task_edit_btn_clicked(self, _) -> None:
        self.toggle_edit_mode()
        # Set entry text and select it
        self.task_edit_entry.get_buffer().props.text = self.task["text"]
        self.task_edit_entry.select_region(0, len(self.task["text"]))
        self.task_edit_entry.grab_focus()

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
        self.accent_colors_popover.popdown()
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

    @Gtk.Template.Callback()
    def on_drag_begin(self, _source, drag) -> bool:
        self.toggle_visibility()
        widget = Gtk.Button(label=self.task["text"])
        icon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(widget)

    @Gtk.Template.Callback()
    def on_drag_cancel(self, *_) -> bool:
        self.toggle_visibility()
        return True

    @Gtk.Template.Callback()
    def on_drag_prepare(self, _source, _x, _y) -> Gdk.ContentProvider:
        value = GObject.Value(Task)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    @Gtk.Template.Callback()
    def on_drop(self, drop, task, _x, _y) -> None:
        if task.__gtype_name__ == "Task":
            data = UserData.get()
            tasks = data["tasks"]
            tasks.insert(tasks.index(self.task) - 1, tasks.pop(tasks.index(task.task)))
            UserData.set(data)
            self.parent.reorder_child_after(task, self)
            self.parent.reorder_child_after(self, task)
        elif task.__gtype_name__ == "SubTask":
            # Remove sub-task
            new_sub_task = task.parent.task["sub"].pop(
                task.parent.task["sub"].index(task.task)
            )
            task.parent.sub_tasks.remove(task)
            task.parent.update_data()
            task.parent.update_statusbar()
            # Add sub-task
            self.task["sub"].append(new_sub_task)
            sub_task = SubTask(new_sub_task, self, self.window)
            self.sub_tasks.append(sub_task)
            sub_task.toggle_visibility()
            self.update_data()
            self.update_statusbar()
            # Expand
            self.expand(True)
