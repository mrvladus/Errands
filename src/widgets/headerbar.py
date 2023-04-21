from gi.repository import Gio, Gtk
from .about_window import AboutWindow
from ..globals import data


class HeaderBar(Gtk.HeaderBar):
    def __init__(self):
        super().__init__()
        self.props.css_classes = ["flat"]

        self.menu = Gio.Menu()

        self.menu.append("About List", "app.about")
        action = Gio.SimpleAction.new("about", None)
        action.connect("activate", lambda *_: AboutWindow().present())
        data["app"].add_action(action)

        self.menu.append("Quit", "app.quit")
        action = Gio.SimpleAction.new("quit", None)
        action.connect("activate", lambda *_: data["app"].quit())
        data["app"].add_action(action)
        data["app"].set_accels_for_action("app.quit", ["<primary>q"])

        self.menu_btn = Gtk.MenuButton(
            icon_name="open-menu-symbolic",
            tooltip_text="Menu",
            menu_model=self.menu,
        )
        self.pack_end(self.menu_btn)
