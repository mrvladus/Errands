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

from gi.repository import Gtk, Adw, GObject, Gdk
from .utils import UserData, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_task.ui")
class SubTask(Gtk.Revealer):
    __gtype_name__ = "SubTask"

    sub_task_main_box = Gtk.Template.Child()
    sub_task_box_rev = Gtk.Template.Child()
    sub_task_text = Gtk.Template.Child()
    sub_task_completed_btn = Gtk.Template.Child()
    sub_task_edit_box_rev = Gtk.Template.Child()
    sub_task_edit_entry = Gtk.Template.Child()

    def __init__(self, task: dict, parent: Gtk.Box, window: Adw.ApplicationWindow):
        super().__init__()
        print("Add sub-task: ", task["text"])
        self.parent = parent
        self.task = task
        self.window = window
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if sub-task completed and toggle checkbox
        self.sub_task_completed_btn.props.active = self.task["completed"]
        # Set text
        self.sub_task_text.props.label = self.text

    def delete(self) -> None:
        print(f"Delete sub-task: {self.task['text']}")
        self.toggle_visibility()
        new_data: dict = UserData.get()
        new_data["history"].append(self.task["id"])
        UserData.set(new_data)
        self.window.update_undo()

    def toggle_edit_box(self) -> None:
        self.sub_task_box_rev.set_reveal_child(
            not self.sub_task_box_rev.get_child_revealed()
        )
        self.sub_task_edit_box_rev.set_reveal_child(
            not self.sub_task_edit_box_rev.get_child_revealed()
        )

    def toggle_visibility(self) -> None:
        self.set_reveal_child(not self.get_child_revealed())

    def update_data(self) -> None:
        new_data: dict = UserData.get()
        for task in new_data["tasks"]:
            if task["id"] == self.parent.task["id"]:
                for i, sub in enumerate(task["sub"]):
                    if sub["id"] == self.task["id"]:
                        task["sub"][i] = self.task
                        UserData.set(new_data)
                        # Update parent data
                        self.parent.task["sub"] = task["sub"]
                        return

    @Gtk.Template.Callback()
    def on_drag_prepare(self, _source, _x, _y):
        value = GObject.Value(SubTask)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    @Gtk.Template.Callback()
    def on_drag_begin(self, _source, drag):
        widget = Gtk.Button(label=self.task["text"])
        icon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(widget)

    @Gtk.Template.Callback()
    def on_drop(self, _drop, sub_task, _x, _y):
        if sub_task.parent != self.parent:
            return
        # Get index
        child = self.parent.sub_tasks.get_first_child()
        self_idx = None
        sub_idx = None
        idx = 0
        while True:
            if child == self:
                self_idx = idx
            if child == sub_task:
                sub_idx = idx
            idx += 1
            child = child.get_next_sibling()
            if not child:
                break
        # Move widget
        self.parent.sub_tasks.reorder_child_after(sub_task, self)
        if self_idx < sub_idx:
            self.parent.sub_tasks.reorder_child_after(self, sub_task)
        # Update data
        self.parent.task["sub"].insert(self_idx, self.parent.task["sub"].pop(sub_idx))
        self.parent.update_data()

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn: Gtk.Button) -> None:
        self.task["completed"] = btn.props.active
        self.update_data()
        self.parent.update_statusbar()
        # Set crosslined text
        if self.task["completed"]:
            self.text = Markup.add_crossline(self.text)
        else:
            self.text = Markup.rm_crossline(self.text)
        self.sub_task_text.props.label = self.text

    @Gtk.Template.Callback()
    def on_sub_task_delete_btn_clicked(self, _) -> None:
        self.delete()
        self.parent.update_statusbar()

    @Gtk.Template.Callback()
    def on_sub_task_edit_btn_clicked(self, _) -> None:
        self.toggle_edit_box()
        # Set entry text and select it
        self.sub_task_edit_entry.get_buffer().props.text = self.task["text"]
        self.sub_task_edit_entry.select_region(0, len(self.task["text"]))
        self.sub_task_edit_entry.grab_focus()

    @Gtk.Template.Callback()
    def on_sub_task_cancel_edit_btn_clicked(self, _) -> None:
        self.toggle_edit_box()

    @Gtk.Template.Callback()
    def on_sub_task_edit(self, entry: Gtk.Entry) -> None:
        old_text: str = self.task["text"]
        new_text: str = entry.get_buffer().props.text
        # Return if text the same or empty
        if new_text == old_text or new_text == "":
            return
        print(f"Change sub-task: '{old_text}' to '{new_text}'")
        self.task["text"] = new_text
        self.task["completed"] = False
        self.update_data()
        self.parent.update_statusbar()
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if text crosslined and toggle checkbox
        self.sub_task_completed_btn.props.active = False
        # Set text
        self.sub_task_text.props.label = self.text
        self.toggle_edit_box()
