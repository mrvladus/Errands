#!/bin/python3
import sys
import signal

# import locale
# import gettext
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Gio, Adw, Gtk, GLib

VERSION = "44.2.3"
pkgdatadir = GLib.get_user_data_dir()

sys.path.insert(1, pkgdatadir)
signal.signal(signal.SIGINT, signal.SIG_DFL)
# localedir = "@localedir@"
# locale.bindtextdomain("list", localedir)
# locale.textdomain("list")
# gettext.install("list", localedir)

# ---------- GLOBAL VARIABLES DICT ---------- #
data = {"todo_list": None}


class HeaderBar(Gtk.HeaderBar):
    def __init__(self):
        super().__init__()
        self.props.css_classes = ["flat"]
        self.menu = Gio.Menu()
        self.menu.append("About List", "app.about")
        self.menu.append("Quit", "app.quit")
        self.menu_btn = Gtk.MenuButton(
            icon_name="open-menu-symbolic",
            tooltip_text="Menu",
            menu_model=self.menu,
        )
        self.pack_end(self.menu_btn)


class Entry(Adw.PreferencesPage):
    def __init__(self):
        super().__init__()
        self.entry = Adw.EntryRow(title="Add new task")
        self.entry.connect("entry-activated", self.on_entry_activated)

        self.group = Adw.PreferencesGroup()
        self.group.add(self.entry)

        self.add(self.group)

    def on_entry_activated(self, entry):
        data["todo_list"].add(Todo(entry.props.text))
        entry.props.text = ""


class TodoList(Adw.PreferencesPage):
    def __init__(self):
        super().__init__()
        data["todo_list"] = self
        self.load_todos()

    def load_todos(self):
        self.add(Todo("hello", ["sub1", "sub2"]))


class Todo(Adw.PreferencesGroup):
    def __init__(self, text, subtodos=[]):
        super().__init__()
        # Sub entry
        sub_entry = Adw.EntryRow(title="Add new sub task")
        sub_entry.connect("entry-activated", self.on_sub_entry_activated)
        # Delete todo button
        del_btn = Gtk.Button(
            icon_name="user-trash-symbolic",
            css_classes=["flat"],
            valign="center",
        )
        del_btn.connect("clicked", self.on_delete)
        # Expander row
        self.row = Adw.ExpanderRow(
            title=text, expanded=True if subtodos != [] else False
        )
        self.row.add_prefix(del_btn)
        # Add entry
        self.row.add_row(sub_entry)
        # Add sub-todos
        for todo in subtodos:
            self.row.add_row(SubTodo(todo, self.row))

        self.add(self.row)

    def on_delete(self, btn):
        data["todo_list"].remove(self)

    def on_sub_entry_activated(self, entry):
        if entry.props.text == "":
            return
        self.row.add_row(SubTodo(entry.props.text))
        entry.props.text = ""


class SubTodo(Adw.ActionRow):
    def __init__(self, text, parent):
        super().__init__(title=text)
        self.parent_todo = parent
        # Delete sub-todo button
        del_btn = Gtk.Button(
            icon_name="user-trash-symbolic",
            css_classes=["flat"],
            valign="center",
        )
        del_btn.connect("clicked", self.on_delete)
        self.add_prefix(del_btn)

    def on_delete(self, btn):
        self.parent_todo.remove(self)


class MainWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="List")
        self.set_size_request(500, 500)
        self.build_interface()
        self.present()

    def build_interface(self):
        box = Gtk.Box(orientation="vertical")
        box.append(HeaderBar())
        box.append(Entry())
        box.append(TodoList())
        self.set_content(box)


class Application(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="io.github.mrvladus.List",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.create_action("quit", lambda *_: self.quit(), ["<primary>q"])
        self.create_action("about", self.on_about_action)
        self.create_action("preferences", self.on_preferences_action)

    def do_activate(self):
        MainWindow(self)

    def on_about_action(self, widget, _):
        about = Adw.AboutWindow(
            transient_for=self.props.active_window,
            application_name="List",
            application_icon="io.github.mrvladus.List",
            developer_name="Vlad Krupinski",
            version=VERSION,
            copyright="© 2023 Vlad Krupinski",
            website="https://github.com/mrvladus/List",
        )
        about.present()

    def on_preferences_action(self, widget, _):
        print("app.preferences action activated")

    def create_action(self, name, callback, shortcuts=None):
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        self.add_action(action)
        if shortcuts:
            self.set_accels_for_action(f"app.{name}", shortcuts)


if __name__ == "__main__":
    # resource = Gio.Resource.load(os.path.join(pkgdatadir, "list.gresource"))
    # resource._register()
    app = Application()
    sys.exit(app.run(sys.argv))
