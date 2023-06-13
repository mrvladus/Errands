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

from gi.repository import Gio, Adw, Gtk
from __main__ import VERSION
from .application import gsettings
from .utils import UserData
from .task import Task
from .preferences import PreferencesWindow


@Gtk.Template(resource_path="/io/github/mrvladus/List/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    delete_completed_tasks_btn_revealer = Gtk.Template.Child()
    delete_completed_tasks_btn = Gtk.Template.Child()
    tasks_list = Gtk.Template.Child()
    status = Gtk.Template.Child()
    about_window = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        # Remember window size
        gsettings.bind("width", self, "default_width", 0)
        gsettings.bind("height", self, "default_height", 0)
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(
            gsettings.get_value("theme").unpack()
        )
        self.get_settings().props.gtk_icon_theme_name = "Adwaita"
        # Create actions for main menu
        self.create_action(
            "preferences",
            lambda *_: PreferencesWindow(self).show(),
            ["<primary>comma"],
        )
        self.create_action("about", self.on_about_action)
        self.create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q"],
        )
        # Load tasks
        self.load_tasks()
        self.update_status()

    def create_action(self, name: str, callback: callable, shortcuts=None) -> None:
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        if shortcuts:
            self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
        self.props.application.add_action(action)

    def load_tasks(self) -> None:
        print("Loading tasks...")
        data: dict = UserData.get()
        if data["tasks"] == []:
            return
        for task in data["tasks"]:
            new_task = Task(task, self)
            self.tasks_list.append(new_task)
            if new_task.get_prev_sibling():
                new_task.get_prev_sibling().update_move_buttons()

    def on_about_action(self, *args) -> None:
        """Show about window"""
        self.about_window.props.version = VERSION
        self.about_window.show()

    def update_status(self) -> None:
        data: dict = UserData.get()
        n_total = 0
        n_completed = 0
        for task in data["tasks"]:
            n_total += 1
            if task["completed"]:
                n_completed += 1
        # Update progress bar
        if n_total > 0:
            self.status.props.fraction = n_completed / n_total
        else:
            self.status.props.fraction = 0

        # Show delete completed button
        self.delete_completed_tasks_btn_revealer.set_reveal_child(n_completed > 0)

    @Gtk.Template.Callback()
    def on_entry_activated(self, entry: Gtk.Entry) -> None:
        text: str = entry.props.text
        new_data: dict = UserData.get()
        # Check for empty string or task exists
        if text == "":
            return
        for task in new_data["tasks"]:
            if task["text"] == text:
                return
        # Add new task
        new_task = {"text": text, "sub": [], "color": "", "completed": False}
        new_data["tasks"].append(new_task)
        UserData.set(new_data)
        task = Task(new_task, self)
        self.tasks_list.append(task)
        if task.get_prev_sibling():
            task.get_prev_sibling().update_move_buttons()
        # Update move buttons
        if len(new_data["tasks"]) > 1:
            self.tasks_list.get_first_child().update_move_buttons()
        # Clear entry
        entry.props.text = ""

    @Gtk.Template.Callback()
    def on_delete_completed_tasks_btn_clicked(self, _) -> None:
        new_data: dict = UserData.get()
        new_data["tasks"] = [t for t in new_data["tasks"] if not t["completed"]]
        UserData.set(new_data)
        # Remove widgets
        to_remove = []
        childrens = self.tasks_list.observe_children()
        for i in range(childrens.get_n_items()):
            child = childrens.get_item(i)
            if child.task["completed"]:
                to_remove.append(child)
        for task in to_remove:
            print("Remove:", task.task["text"])
            self.tasks_list.remove(task)
        self.update_status()
