from gi.repository import Gio, Gtk


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
