from gi import require_version

require_version("Gtk", "4.0")
require_version("Adw", "1")

from gi.repository import Gio, Adw, Gtk, Gdk
from .globals import APP_ID, data
from .data import InitData
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
        self.load_gresource()
        self.load_css()
        InitData()
        MainWindow(self).present()

    def load_gresource(self):
        resource = Gio.Resource.load("/app/share/list/list.gresource")
        resource._register()

    def load_css(self):
        css_provider = Gtk.CssProvider()
        css_provider.load_from_resource("/io/github/mrvladus/List/styles.css")
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )
