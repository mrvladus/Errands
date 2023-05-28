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
from .application import gsettings, VERSION
from .utils import UserData
from .task import Task
from .preferences import PreferencesWindow


@Gtk.Template(resource_path="/io/github/mrvladus/List/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    box = Gtk.Template.Child()
    tasks_list = Gtk.Template.Child()
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

    def create_action(self, name, callback, shortcuts=None):
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        if shortcuts:
            self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
        self.props.application.add_action(action)

    def load_tasks(self):
        print("Loading tasks...")
        data = UserData.get()
        if data["tasks"] == []:
            return
        for task in data["tasks"]:
            new_task = Task(task, self.tasks_list)
            self.tasks_list.append(new_task)
            if new_task.get_prev_sibling():
                new_task.get_prev_sibling().update_move_buttons()

    def on_about_action(self, *args):
        self.about_window.props.version = VERSION
        self.about_window.show()

    @Gtk.Template.Callback()
    def on_entry_activated(self, entry):
        text = entry.props.text
        new_data = UserData.get()
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
        self.tasks_list.append(Task(new_task, self.tasks_list))
        # Update move buttons
        if len(new_data["tasks"]) > 1:
            self.tasks_list.get_first_child().update_move_buttons()
        # Clear entry
        entry.props.text = ""
