from gi.repository import Gio, Gtk


class HeaderBar(Gtk.HeaderBar):
    def __init__(self):
        super().__init__()
        self.props.css_classes = ["flat"]

        self.menu = Gio.Menu()

        self.menu.append("About List", "app.about")
        action = Gio.SimpleAction.new("app.about", None)
        self.menu.append("Quit", "app.quit")
        action.connect("activate", self.on_about_action)

        self.menu_btn = Gtk.MenuButton(
            icon_name="open-menu-symbolic",
            tooltip_text="Menu",
            menu_model=self.menu,
        )
        self.pack_end(self.menu_btn)

    def on_about_action(self, widget, _):
        from .about_window import AboutWindow

        AboutWindow(self.props.active_window).present()
