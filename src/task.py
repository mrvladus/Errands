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
from gi.repository import Gtk, Adw
from .main import gsettings
from .sub_task import SubTask
from .utils import Markup, UserData


@Gtk.Template(resource_path="/io/github/mrvladus/List/task.ui")
class Task(Adw.Bin):
    __gtype_name__ = "Task"

    # Template items
    task_box = Gtk.Template.Child()
    task_popover = Gtk.Template.Child()
    task_text = Gtk.Template.Child()
    expand_btn = Gtk.Template.Child()
    task_completed_btn = Gtk.Template.Child()
    task_delete_btn = Gtk.Template.Child()
    task_status = Gtk.Template.Child()
    sub_tasks_revealer = Gtk.Template.Child()
    sub_tasks = Gtk.Template.Child()
    accent_colors_btn = Gtk.Template.Child()
    task_text_box = Gtk.Template.Child()
    task_cancel_edit_btn = Gtk.Template.Child()
    task_edit_entry = Gtk.Template.Child()
    task_edit_btn = Gtk.Template.Child()
    delete_completed_btn = Gtk.Template.Child()
    delete_completed_btn_revealer = Gtk.Template.Child()

    def __init__(self, task: dict, parent):
        super().__init__()
        print("Add task:", task["text"])
        self.parent = parent
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
            self.task_box.add_css_class(f'task_{self.task["color"]}')
        # Expand if sub-tasks exists
        if self.task["sub"] != [] and gsettings.get_value("tasks-expanded").unpack():
            self.expand(True)
        # Add sub tasks
        for task in self.task["sub"]:
            self.sub_tasks.append(SubTask(task, self))
        self.update_statusbar()

    def expand(self, expand: bool):
        self.sub_tasks_revealer.set_reveal_child(expand)
        if expand:
            self.expand_btn.set_icon_name("go-up-symbolic")
        else:
            self.expand_btn.set_icon_name("go-down-symbolic")

    def toggle_edit_mode(self):
        visible = self.task_text_box.props.visible
        # Hide widgets
        self.task_delete_btn.props.visible = not visible
        self.task_text_box.props.visible = not visible
        self.expand_btn.props.visible = not visible
        self.task_completed_btn.props.visible = not visible
        self.accent_colors_btn.props.visible = not visible
        self.task_edit_btn.props.visible = not visible
        # Show widgets
        self.task_cancel_edit_btn.props.visible = visible
        self.task_edit_entry.props.visible = visible

    def update_statusbar(self):
        n_completed = 0
        n_total = 0
        for sub in self.task["sub"]:
            n_total += 1
            if sub["completed"]:
                n_completed += 1
        if n_total > 0:
            self.task_status.props.visible = True
            self.task_status.set_label(_("Completed:") + f" {n_completed} / {n_total}")
        else:
            self.task_status.props.visible = False
        # Show delete completed button
        if n_completed > 0:
            self.delete_completed_btn_revealer.set_reveal_child(True)
        else:
            self.delete_completed_btn_revealer.set_reveal_child(False)

    def update_task(self, new_task: dict):
        new_data = UserData.get()
        for i, task in enumerate(new_data["tasks"]):
            if task["text"] == self.task["text"]:
                new_data["tasks"][i] = new_task
                UserData.set(new_data)
                return

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_task_delete(self, _):
        new_data: dict = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == self.task["text"]:
                new_data["tasks"].remove(task)
                break
        UserData.set(new_data)
        self.parent.remove(self)

    @Gtk.Template.Callback()
    def on_delete_completed_btn_clicked(self, _):
        # Remove data
        self.task["sub"] = [sub for sub in self.task["sub"] if not sub["completed"]]
        new_data = UserData.get()
        for i, task in enumerate(new_data["tasks"]):
            if task["text"] == self.task["text"]:
                new_data["tasks"][i] = self.task
                UserData.set(new_data)
                break
        # Remove widgets
        to_remove = []
        childrens = self.sub_tasks.observe_children()
        for i in range(childrens.get_n_items()):
            child = childrens.get_item(i)
            if child.task["completed"]:
                to_remove.append(child)
        for task in to_remove:
            print("Remove:", task.task["text"])
            self.sub_tasks.remove(task)
        # Update statusbar
        self.update_statusbar()

    @Gtk.Template.Callback()
    def on_task_completed_btn_toggled(self, btn):
        if btn.props.active:
            self.task = {
                "text": self.task["text"],
                "completed": True,
                "sub": self.task["sub"],
                "color": self.task["color"],
            }
            self.update_task(self.task)
            self.text = Markup.add_crossline(self.text)
        else:
            self.task = {
                "text": self.task["text"],
                "completed": False,
                "sub": self.task["sub"],
                "color": self.task["color"],
            }
            self.update_task(self.task)
            self.text = Markup.rm_crossline(self.text)
        self.task_text.props.label = self.text

    @Gtk.Template.Callback()
    def on_expand_btn_clicked(self, _):
        """Expand task row"""
        if self.sub_tasks_revealer.get_child_revealed():
            self.expand(False)
        else:
            self.expand(True)

    @Gtk.Template.Callback()
    def on_sub_task_added(self, entry):
        # Return if entry is empty
        if entry.get_buffer().props.text == "":
            return
        # Return if task exists
        for sub in self.task["sub"]:
            if sub["text"] == entry.get_buffer().props.text:
                return
        # Add new sub-task
        new_sub_task = {
            "text": entry.get_buffer().props.text,
            "completed": False,
        }
        self.task["sub"].append(new_sub_task)
        self.task = {
            "text": self.task["text"],
            "completed": self.task["completed"],
            "color": self.task["color"],
            "sub": self.task["sub"],
        }
        self.update_task(self.task)
        # Add row
        self.sub_tasks.append(SubTask(new_sub_task, self))
        self.update_statusbar()
        # Clear entry
        entry.get_buffer().props.text = ""

    @Gtk.Template.Callback()
    def on_task_edit_btn_clicked(self, _):
        self.toggle_edit_mode()
        # Set entry text and select it
        self.task_edit_entry.get_buffer().props.text = self.task["text"]
        self.task_edit_entry.select_region(0, len(self.task["text"]))
        self.task_edit_entry.grab_focus()

    @Gtk.Template.Callback()
    def on_task_cancel_edit_btn_clicked(self, _):
        self.toggle_edit_mode()

    @Gtk.Template.Callback()
    def on_task_edit(self, entry):
        old_text = self.task["text"]
        new_text = entry.get_buffer().props.text
        # Return if text the same or empty
        if new_text == old_text or new_text == "":
            return
        # Return if task exists
        new_data = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == new_text:
                return
        # Change task
        print(f"Change '{old_text}' to '{new_text}'")
        # Set new text
        self.task = {
            "text": new_text,
            "sub": self.task["sub"],
            "completed": False,
            "color": self.task["color"],
        }
        # Set new data
        for i, task in enumerate(new_data["tasks"]):
            if task["text"] == old_text:
                new_data["tasks"][i] = self.task
                UserData.set(new_data)
                break
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if text crosslined and toggle checkbox
        self.task_completed_btn.props.active = False
        # Set text
        self.task_text.props.label = self.text
        self.toggle_edit_mode()

    @Gtk.Template.Callback()
    def on_style_selected(self, btn):
        """Apply accent color"""
        self.task_popover.popdown()
        for i in btn.get_css_classes():
            color = ""
            if i.startswith("btn_"):
                color = i.split("_")[1]
                break
        self.task_box.set_css_classes(
            ["card"] if color == "" else ["card", f"task_{color}"]
        )
        # Set new color
        self.task = {
            "text": self.task["text"],
            "sub": self.task["sub"],
            "completed": self.task["completed"],
            "color": color,
        }
        self.update_task(self.task)
