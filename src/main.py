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

import os
import json
from gi import require_version

require_version("Gtk", "4.0")
require_version("Adw", "1")

from gi.repository import Gio, Adw, Gtk, Gdk, GLib


VERSION = "44.4.3"
APP_ID = "io.github.mrvladus.List"
gsettings = Gio.Settings.new(APP_ID)


class Application(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
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
    todo_list = Gtk.Template.Child()
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
        )
        self.create_action("about", self.on_about_action)
        self.create_action("quit", lambda *_: self.props.application.quit())
        # Load tasks
        self.load_todos()

    def create_action(self, name, callback, shortcuts=None):
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        self.props.application.add_action(action)

    def load_todos(self):
        data = UserData.get()
        if data["todos"] == {}:
            return
        for todo in data["todos"]:
            self.todo_list.add(
                Todo(
                    todo,
                    data["todos"][todo]["color"],
                    data["todos"][todo]["sub"],
                    self.todo_list,
                )
            )

    def on_about_action(self, *args):
        self.about_window.props.version = VERSION
        self.about_window.show()

    @Gtk.Template.Callback()
    def on_entry_activated(self, entry):
        text = entry.props.text
        new_data = UserData.get()
        # Check for empty string or todo exists
        if text == "" or text in new_data["todos"]:
            return
        # Add new todo
        new_data["todos"][text] = {"sub": [], "color": ""}
        UserData.set(new_data)
        self.todo_list.add(Todo(text, "", [], self.todo_list))
        # Clear entry
        entry.props.text = ""


@Gtk.Template(resource_path="/io/github/mrvladus/List/preferences_window.ui")
class PreferencesWindow(Adw.PreferencesWindow):
    __gtype_name__ = "PreferencesWindow"

    system_theme = Gtk.Template.Child()
    light_theme = Gtk.Template.Child()
    dark_theme = Gtk.Template.Child()

    def __init__(self, win):
        super().__init__()
        self.props.transient_for = win
        theme = gsettings.get_value("theme").unpack()
        if theme == 0:
            self.system_theme.props.active = True
        if theme == 1:
            self.light_theme.props.active = True
        if theme == 4:
            self.dark_theme.props.active = True

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


@Gtk.Template(resource_path="/io/github/mrvladus/List/todo.ui")
class Todo(Adw.PreferencesGroup):
    __gtype_name__ = "Todo"

    task_row = Gtk.Template.Child()
    task_popover = Gtk.Template.Child()

    def __init__(self, text, color, sub_todos, parent):
        super().__init__()
        self.text = text
        self.parent = parent
        old_text = self.text
        # Detect if text was escaped
        if (
            "&amp;" in old_text
            or "&lt;" in old_text
            or "&gt;" in old_text
            or "&#39;" in old_text
            or "&apos;" in old_text
        ):
            self.text = old_text
        # If not then escape it
        else:
            self.text = GLib.markup_escape_text(self.text)
        self.update_data(old_text, self.text)
        self.task_row.props.title = self.text
        # Set accent color
        if color != "":
            self.task_row.add_css_class(f"row_{color}")
        # Add sub tasks
        for todo in sub_todos:
            self.task_row.add_row(SubTodo(todo, self.task_row))
        # Expand if sub tasks exists
        if sub_todos != []:
            self.task_row.props.expanded = True

    def update_data(self, old_text: str, new_text: str):
        new_data = UserData.get()
        # Create new dict and change todo text
        tmp = UserData.default_data
        for key in new_data["todos"]:
            if key == old_text:
                tmp["todos"][new_text] = new_data["todos"][old_text]
            else:
                tmp["todos"][key] = new_data["todos"][key]
        UserData.set(tmp)

    @Gtk.Template.Callback()
    def on_task_delete(self, _):
        self.task_popover.popdown()
        new_data = UserData.get()
        new_data["todos"].pop(self.task_row.props.title)
        UserData.set(new_data)
        self.parent.remove(self)

    @Gtk.Template.Callback()
    def on_task_edited(self, entry):
        # Get old and new text
        old_text = self.task_row.props.title
        new_text = GLib.markup_escape_text(entry.get_buffer().props.text)
        new_data = UserData.get()
        if old_text == new_text or new_text in new_data["todos"] or new_text == "":
            return
        self.update_data(old_text, new_text)
        # Set new title
        self.task_row.props.title = new_text
        # Clear entry
        entry.get_buffer().props.text = ""
        # Hide popup
        self.task_popover.popdown()

    @Gtk.Template.Callback()
    def on_style_selected(self, btn):
        self.task_popover.popdown()
        for i in btn.get_css_classes():
            color = ""
            if i.startswith("btn_"):
                color = i.split("_")[1]
                break
        self.task_row.set_css_classes(
            ["expander"] if color == "" else ["expander", f"row_{color}"]
        )
        new_data = UserData.get()
        new_data["todos"][self.task_row.props.title]["color"] = color
        UserData.set(new_data)

    @Gtk.Template.Callback()
    def on_sub_entry_activated(self, entry):
        todo = self.task_row.props.title
        sub_todo = entry.props.text
        new_data = UserData.get()
        if sub_todo in new_data["todos"][todo]["sub"] or sub_todo == "":
            return
        new_data["todos"][todo]["sub"].append(sub_todo)
        UserData.set(new_data)
        self.task_row.add_row(SubTodo(sub_todo, self.task_row))
        entry.props.text = ""


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_todo.ui")
class SubTodo(Adw.ActionRow):
    __gtype_name__ = "SubTodo"

    sub_task_popover = Gtk.Template.Child()
    sub_task_completed_btn = Gtk.Template.Child()

    def __init__(self, text, parent):
        super().__init__()
        self.text = text
        self.parent = parent
        # Escape markup symbols
        if "<s>" in text:
            self.props.title = self.text
            self.sub_task_completed_btn.props.active = True
        else:
            old_text = self.text
            # Detect if text was escaped
            if (
                "&amp;" in old_text
                or "&lt;" in old_text
                or "&gt;" in old_text
                or "&#39;" in old_text
                or "&apos;" in old_text
            ):
                self.text = old_text
            # If not then escape it
            else:
                self.text = GLib.markup_escape_text(self.text)
            self.update_data(old_text, self.text)
            self.props.title = self.text

    def update_data(self, old_text: str, new_text: str):
        new_data = UserData.get()
        idx = new_data["todos"][self.parent.props.title]["sub"].index(old_text)
        new_data["todos"][self.parent.props.title]["sub"][idx] = new_text
        UserData.set(new_data)

    @Gtk.Template.Callback()
    def on_sub_task_delete(self, _):
        self.sub_task_popover.popdown()
        new_data = UserData.get()
        new_data["todos"][self.parent.get_title()]["sub"].remove(self.props.title)
        UserData.set(new_data)
        self.parent.remove(self)

    @Gtk.Template.Callback()
    def on_sub_task_edit(self, entry):
        old_text = self.props.title
        new_text = GLib.markup_escape_text(entry.get_buffer().props.text)
        if (
            new_text == old_text
            or new_text == ""
            or new_text in UserData.get()["todos"][self.parent.props.title]["sub"]
        ):
            return
        # Change sub-todo
        self.update_data(old_text, new_text)
        # Set new title
        self.props.title = new_text
        # Mark as uncompleted
        self.sub_task_completed_btn.props.active = False
        # Clear entry
        entry.get_buffer().props.text = ""
        # Hide popup
        self.sub_task_popover.popdown()

    @Gtk.Template.Callback()
    def on_sub_task_complete_toggle(self, btn):
        old_text = self.props.title
        active = btn.props.active
        # Ignore at app launch
        if "<s>" in old_text and active:
            return
        # Ignore when sub task was just edited
        if not active and "<s>" not in old_text:
            return
        new_sub_task = f"<s>{old_text}</s>" if active else old_text[3:-4]
        self.props.title = new_sub_task
        # Save new sub task
        self.update_data(old_text, new_sub_task)


class UserData:
    """Class for accessing data file with user tasks"""

    data_dir = GLib.get_user_data_dir() + "/list"
    default_data = {
        "version": VERSION,
        "todos": {},
    }

    # Create data dir and data.json file
    @classmethod
    def init(self):
        if not os.path.exists(self.data_dir):
            os.mkdir(self.data_dir)
        if not os.path.exists(self.data_dir + "/data.json"):
            with open(self.data_dir + "/data.json", "w+") as f:
                json.dump(self.default_data, f)
            self.convert()

    # Load user data from json
    @classmethod
    def get(self):
        if not os.path.exists(self.data_dir + "/data.json"):
            self.init()
        with open(self.data_dir + "/data.json", "r") as f:
            data = json.load(f)
            return data

    # Save user data to json
    @classmethod
    def set(self, data: dict):
        with open(self.data_dir + "/data.json", "w") as f:
            json.dump(data, f)

    # Port todos from older versions (for updates)
    @classmethod
    def convert(self):
        # 44.1.x
        todos_v1 = gsettings.get_value("todos").unpack()
        if todos_v1 != []:
            new_data = self.get()
            for todo in todos_v1:
                new_data["todos"][todo] = {
                    "sub": [],
                    "color": "",
                }
            self.set(new_data)
            gsettings.set_value("todos", GLib.Variant("as", []))
        # 44.2.x
        todos_v2 = gsettings.get_value("todos-v2").unpack()
        if todos_v2 != []:
            new_data = self.get()
            for todo in todos_v2:
                new_data["todos"][todo[0]] = {
                    "sub": [i for i in todo[1:]],
                    "color": "",
                }
            self.set(new_data)
            gsettings.set_value("todos-v2", GLib.Variant("aas", []))
