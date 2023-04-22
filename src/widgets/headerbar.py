from gi.repository import Gio, Gtk, Adw
from ..globals import data, APP_ID, VERSION


class HeaderBar(Gtk.HeaderBar):
    def __init__(self):
        super().__init__()
        self.props.css_classes = ["flat"]

        self.menu = Gio.Menu()

        self.menu.append("About List", "app.about")
        action = Gio.SimpleAction.new("about", None)
        action.connect("activate", self.on_about_action)
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

    def on_about_action(self, *args):
        win = Adw.AboutWindow(
            transient_for=data["app"].props.active_window,
            application_name="List",
            application_icon=APP_ID,
            developer_name="Vlad Krupinski",
            version=VERSION,
            copyright="© 2023 Vlad Krupinski",
            website="https://github.com/mrvladus/List",
        )
        win.present()
