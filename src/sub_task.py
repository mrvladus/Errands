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

# from gettext import gettext as _
from gi.repository import Gtk, GObject, Gdk, Gio, GLib
from .utils import Log, UserData, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/sub_task.ui")
class SubTask(Gtk.Revealer):
    __gtype_name__ = "SubTask"

    sub_task_box_rev: Gtk.Revealer = Gtk.Template.Child()
    task_text: Gtk.Label = Gtk.Template.Child()
    task_completed_btn: Gtk.Button = Gtk.Template.Child()
    sub_task_edit_box_rev: Gtk.Revealer = Gtk.Template.Child()
    sub_task_edit_entry: Gtk.Entry = Gtk.Template.Child()

    def __init__(self, task: dict, parent: Gtk.Box, window):
        super().__init__()
        Log.info("Add sub-task: " + task["text"])
        self.parent = parent
        self.task = task
        self.window = window
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if sub-task completed and toggle checkbox
        self.task_completed_btn.props.active = self.task["completed"]
        # Set text
        self.task_text.props.label = self.text
        self.add_actions()

    def add_actions(self):
        group = Gio.SimpleActionGroup.new()
        self.insert_action_group("sub_task", group)

        def add_action(name: str, callback):
            action = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        add_action("delete", self.delete)
        add_action("edit", self.edit)
        add_action("copy", self.copy)

    def copy(self, *_) -> None:
        Log.info("Copy to clipboard: " + self.task["text"])
        clp: Gdk.Clipboard = Gdk.Display.get_default().get_clipboard()
        clp.set(self.task["text"])
        self.window.add_toast(self.window.toast_copied)

    def delete(self, *_, update_sts: bool = True) -> None:
        Log.info(f"Delete sub-task: {self.task['text']}")
        self.toggle_visibility()
        self.task["deleted"] = True
        self.update_data()
        # Don't update if called externally
        if update_sts:
            self.parent.update_statusbar()
        self.window.trash_add(self.task)

    def edit(self, *_) -> None:
        self.toggle_edit_box()
        # Set entry text and select it
        self.sub_task_edit_entry.get_buffer().props.text = self.task["text"]
        self.sub_task_edit_entry.select_region(0, len(self.task["text"]))
        self.sub_task_edit_entry.grab_focus()

    def toggle_edit_box(self) -> None:
        self.sub_task_box_rev.set_reveal_child(
            not self.sub_task_box_rev.get_child_revealed()
        )
        self.sub_task_edit_box_rev.set_reveal_child(
            not self.sub_task_edit_box_rev.get_child_revealed()
        )

    def toggle_visibility(self, on: bool = False) -> None:
        if on:
            self.set_reveal_child(True)
        else:
            self.set_reveal_child(not self.get_child_revealed())

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
    def on_drag_begin(self, _source, drag: Gdk.Drag) -> bool:
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
        value = GObject.Value(SubTask)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    @Gtk.Template.Callback()
    def on_drop(self, _drop, task, _x, _y) -> None:
        if task == self:
            return
        # If task has different parent
        if task.task["parent"] != self.task["parent"]:
            # Set parent
            data: dict = UserData.get()
            for t in data["tasks"]:
                if t["id"] == task.task["id"]:
                    t["parent"] = task.task["parent"] = self.task["parent"]
                    break
            UserData.set(data)

            def check_visible():
                if task.get_child_revealed():
                    return True
                else:
                    task.parent.sub_tasks.remove(task)
                    task.parent.update_data()
                    task.parent.update_statusbar()
                    return False

            # Hide task
            task.toggle_visibility()
            GLib.timeout_add(100, check_visible)
            self.parent.update_data()
            self.parent.update_statusbar()
            # Insert new sub-task
            new_sub_task = SubTask(task.task.copy(), self.parent, self.window)
            self.parent.sub_tasks.insert_child_after(
                new_sub_task,
                self,
            )
            self.parent.sub_tasks.reorder_child_after(self, new_sub_task)
            new_sub_task.toggle_visibility()
        else:
            # Move widget
            self.parent.sub_tasks.reorder_child_after(task, self)
            self.parent.sub_tasks.reorder_child_after(self, task)
            # Update data
            data: dict = UserData.get()
            old_task = data["tasks"].pop(data["tasks"].index(task.task))
            old_task["parent"] = self.task["parent"]
            data["tasks"].insert(data["tasks"].index(self.task), old_task)
            UserData.set(data)

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn: Gtk.Button) -> None:
        self.task["completed"] = btn.props.active
        self.update_data()
        self.parent.update_statusbar()
        # Set crosslined text
        if btn.props.active:
            self.text = Markup.add_crossline(self.text)
            self.task_text.add_css_class("dim-label")
        else:
            self.text = Markup.rm_crossline(self.text)
            self.task_text.remove_css_class("dim-label")
        self.task_text.props.label = self.text

    @Gtk.Template.Callback()
    def on_sub_task_cancel_edit_btn_clicked(self, *_) -> None:
        self.toggle_edit_box()

    @Gtk.Template.Callback()
    def on_sub_task_edit(self, entry: Gtk.Entry) -> None:
        old_text: str = self.task["text"]
        new_text: str = entry.get_buffer().props.text
        # Return if text the same or empty
        if new_text == old_text or new_text == "":
            return
        Log.info(f"Change sub-task: '{old_text}' to '{new_text}'")
        self.task["text"] = new_text
        self.task["completed"] = False
        self.update_data()
        self.parent.update_statusbar()
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if text crosslined and toggle checkbox
        self.task_completed_btn.props.active = False
        # Set text
        self.task_text.props.label = self.text
        self.toggle_edit_box()
