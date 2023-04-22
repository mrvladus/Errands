from gi.repository import Adw, Gtk
from .headerbar import HeaderBar
from .entry import Entry
from .todolist import TodoList
from ..globals import data


class MainWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="List")
        self.set_size_request(500, 500)
        data["gsettings"].bind("width", self, "default_width", 0)
        data["gsettings"].bind("height", self, "default_height", 0)
        self.build_interface()

    def build_interface(self):
        box = Gtk.Box(orientation="vertical")
        box.append(HeaderBar())
        box.append(Entry())
        box.append(TodoList())
        self.set_content(box)
