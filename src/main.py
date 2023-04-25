from gi import require_version

require_version("Gtk", "4.0")
require_version("Adw", "1")

from gi.repository import Gio, Adw, Gtk, Gdk
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
        MainWindow(self).present()

    def load_css(self):
        css_provider = Gtk.CssProvider()
        css_provider.load_from_path("/app/share/list/styles.css")
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )
