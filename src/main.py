from gi import require_version

require_version("Gtk", "4.0")
require_version("Adw", "1")

from gi.repository import Gio, Adw
from .globals import APP_ID, data
from .widgets.main_window import MainWindow


class Application(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        data["app"] = self

    def do_activate(self):
        data["gsettings"] = Gio.Settings.new("")
        MainWindow(self).present()
