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

from gi import require_version

require_version("Gtk", "4.0")
require_version("Adw", "1")

from gi.repository import Gio, Adw, Gtk, Gdk, GLib

# Global data
VERSION = "44.5"
APP_ID = "io.github.mrvladus.List"
gsettings = Gio.Settings.new(APP_ID)

# Import widgets
from .utils import UserData
from .task import Task


class Application(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        print("Activate app...")
        # Initialize data.json file
        UserData.init()
        # Load css styles
        css_provider = Gtk.CssProvider()
        css_provider.load_from_resource("/io/github/mrvladus/List/styles.css")
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )
        # Show window
        Window(application=self).present()


@Gtk.Template(resource_path="/io/github/mrvladus/List/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    box = Gtk.Template.Child()
    tasks_list = Gtk.Template.Child()
    about_window = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        print("Load window...")
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
        self.load_todos()

    def create_action(self, name, callback, shortcuts=None):
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        if shortcuts:
            self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
        self.props.application.add_action(action)

    def load_todos(self):
        print("Loading tasks...")
        data = UserData.get()
        if data["tasks"] == []:
            return
        for task in data["tasks"]:
            self.tasks_list.append(Task(task, self.tasks_list))

    def on_about_action(self, *args):
        self.about_window.props.version = VERSION
        self.about_window.show()

    @Gtk.Template.Callback()
    def on_entry_activated(self, entry):
        text = entry.props.text
        new_data = UserData.get()
        # Check for empty string or todo exists
        if text == "":
            return
        for task in new_data["tasks"]:
            if task["text"] == text:
                return
        # Add new todo
        new_task = {"text": text, "sub": [], "color": "", "completed": False}
        new_data["tasks"].append(new_task)
        UserData.set(new_data)
        self.tasks_list.append(Task(new_task, self.tasks_list))
        # Clear entry
        entry.props.text = ""


@Gtk.Template(resource_path="/io/github/mrvladus/List/preferences_window.ui")
class PreferencesWindow(Adw.PreferencesWindow):
    __gtype_name__ = "PreferencesWindow"

    system_theme = Gtk.Template.Child()
    light_theme = Gtk.Template.Child()
    dark_theme = Gtk.Template.Child()
    tasks_expanded = Gtk.Template.Child()

    def __init__(self, win):
        super().__init__()
        self.props.transient_for = win
        # Setup theme
        theme = gsettings.get_value("theme").unpack()
        if theme == 0:
            self.system_theme.props.active = True
        if theme == 1:
            self.light_theme.props.active = True
        if theme == 4:
            self.dark_theme.props.active = True
        # Setup tasks
        expanded = gsettings.get_value("tasks-expanded").unpack()
        self.tasks_expanded.props.active = expanded

    @Gtk.Template.Callback()
    def on_theme_change(self, btn):
        id = btn.get_buildable_id()
        if id == "system_theme":
            theme = 0
        elif id == "light_theme":
            theme = 1
        elif id == "dark_theme":
            theme = 4
        Adw.StyleManager.get_default().set_color_scheme(theme)
        gsettings.set_value("theme", GLib.Variant("i", theme))

    @Gtk.Template.Callback()
    def on_tasks_expanded_toggle(self, widget, state):
        gsettings.set_value("tasks-expanded", GLib.Variant("b", state))
