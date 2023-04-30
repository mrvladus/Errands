from gi import require_version

require_version("Gtk", "4.0")
require_version("Adw", "1")

from gi.repository import Gio, Adw, Gtk, Gdk, GLib
from .globals import APP_ID, data, VERSION
from .data import UserData
from .widgets.main_window import MainWindow


class Application(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        data["app"] = self
        data["gsettings"] = Gio.Settings.new(APP_ID)
        UserData.init()
        self.load_css()
        Window(application=self).present()

    def load_css(self):
        css_provider = Gtk.CssProvider()
        css_provider.load_from_resource("/io/github/mrvladus/List/styles.css")
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )


@Gtk.Template(resource_path="/io/github/mrvladus/List/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    box = Gtk.Template.Child()
    todo_list = Gtk.Template.Child()
    about_window = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.setup_size()
        self.setup_theme()
        self.setup_actions()
        self.load_todos()

    def setup_size(self):
        # Remember window size
        data["gsettings"].bind("width", self, "default_width", 0)
        data["gsettings"].bind("height", self, "default_height", 0)

    def setup_theme(self):
        Adw.StyleManager.get_default().set_color_scheme(
            data["gsettings"].get_value("theme").unpack()
        )

    def setup_actions(self):
        self.create_action("preferences", lambda *_: PreferencesWindow(self).show())
        self.create_action("about", lambda *_: self.about_window.show())
        self.create_action(
            "quit", lambda *_: self.props.application.quit(), ["<primary>q"]
        )

    def create_action(self, name, callback, shortcuts=None):
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        self.props.application.add_action(action)
        if shortcuts:
            self.props.application.set_accels_for_action(f"app.{name}", shortcuts)

    def load_todos(self):
        data = UserData.get()
        if data["todos"] != {}:
            for todo in data["todos"]:
                self.todo_list.add(
                    Todo(
                        todo,
                        data["todos"][todo]["color"],
                        data["todos"][todo]["sub"],
                        self.todo_list,
                    )
                )

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
        theme = data["gsettings"].get_value("theme").unpack()
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
        data["gsettings"].set_value("theme", GLib.Variant("i", theme))


@Gtk.Template(resource_path="/io/github/mrvladus/List/todo.ui")
class Todo(Adw.PreferencesGroup):
    __gtype_name__ = "Todo"

    task_row = Gtk.Template.Child()
    task_popover = Gtk.Template.Child()

    def __init__(self, text, color, sub_todos, parent):
        super().__init__()
        self.parent = parent
        self.task_row.props.title = text
        if color != "":
            self.task_row.add_css_class(f"row_{color}")
        for todo in sub_todos:
            self.task_row.add_row(SubTodo(todo, self.task_row))
        if sub_todos != []:
            self.task_row.props.expanded = True

    @Gtk.Template.Callback()
    def on_task_delete(self, _):
        self.task_popover.popdown()
        new_data = UserData.get()
        new_data["todos"].pop(self.task_row.props.title)
        UserData.set(new_data)
        self.parent.remove(self)

    @Gtk.Template.Callback()
    def on_task_edit(self, entry):
        # Hide popup
        self.task_popover.popdown()
        # Get old and new text
        old_text = self.task_row.props.title
        new_text = entry.get_buffer().props.text
        new_data = UserData.get()
        if old_text == new_text or new_text in new_data["todos"] or new_text == "":
            return
        # Create new dict and change todo text
        tmp = UserData.default_data
        for key in new_data["todos"]:
            if key == old_text:
                tmp["todos"][new_text] = new_data["todos"][old_text]
            else:
                tmp["todos"][key] = new_data["todos"][key]
        UserData.set(tmp)
        # Set new title
        self.task_row.props.title = new_text
        # Clear entry
        entry.get_buffer().props.text = ""

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
        pass


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_todo.ui")
class SubTodo(Adw.ActionRow):
    __gtype_name__ = "SubTodo"

    sub_task_popover = Gtk.Template.Child()

    def __init__(self, text, parent):
        super().__init__()
        self.parent = parent
        self.props.title = text

    @Gtk.Template.Callback()
    def on_sub_task_delete(self, _):
        self.sub_task_popover.popdown()
        new_data = UserData.get()
        new_data["todos"][self.parent.get_title()]["sub"].remove(self.props.title)
        UserData.set(new_data)
        self.parent.remove(self)

    @Gtk.Template.Callback()
    def on_sub_task_edit(self, entry):
        pass
