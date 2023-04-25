from gi.repository import Gio, Gtk, Adw, GLib
from ..globals import data, APP_ID, VERSION


class PreferencesWindow(Adw.PreferencesWindow):
    def __init__(self):
        super().__init__()
        self.props.transient_for = data["app"].props.active_window
        page = Adw.PreferencesPage()
        self.add(page)
        # --- Theme group --- #
        theme_group = Adw.PreferencesGroup(title="Application theme")
        page.add(theme_group)
        # Get theme from gsettings
        theme: int = data["gsettings"].get_value("theme").unpack()
        # System theme
        sys_btn = Gtk.CheckButton(active=(theme == 0))
        sys_theme = Adw.ActionRow(title="System", activatable_widget=sys_btn)
        sys_theme.add_prefix(sys_btn)
        sys_theme.add_suffix(Gtk.Image(icon_name="emblem-system-symbolic"))
        sys_theme.connect("activated", self.on_theme_change, 0)
        theme_group.add(sys_theme)
        # Light theme
        light_btn = Gtk.CheckButton(active=(theme == 1), group=sys_btn)
        light_theme = Adw.ActionRow(title="Light", activatable_widget=light_btn)
        light_theme.add_prefix(light_btn)
        light_theme.add_suffix(Gtk.Image(icon_name="display-brightness-symbolic"))
        light_theme.connect("activated", self.on_theme_change, 1)
        theme_group.add(light_theme)
        # Dark theme
        dark_btn = Gtk.CheckButton(active=(theme == 4), group=sys_btn)
        dark_theme = Adw.ActionRow(title="Dark", activatable_widget=dark_btn)
        dark_theme.add_prefix(dark_btn)
        dark_theme.add_suffix(Gtk.Image(icon_name="weather-clear-night-symbolic"))
        dark_theme.connect("activated", self.on_theme_change, 4)
        theme_group.add(dark_theme)
        self.present()

    def on_theme_change(self, _, theme: int):
        Adw.StyleManager.get_default().set_color_scheme(theme)
        data["gsettings"].set_value("theme", GLib.Variant("i", theme))


class HeaderBar(Gtk.HeaderBar):
    def __init__(self):
        super().__init__()
        self.props.css_classes = ["flat"]

        self.menu = Gio.Menu()

        self.menu.append("Preferences", "app.preferences")
        self.create_action("preferences", lambda *_: PreferencesWindow())

        self.menu.append("About List", "app.about")
        self.create_action("about", self.on_about_action)

        self.menu.append("Quit", "app.quit")
        self.create_action("quit", lambda *_: data["app"].quit(), ["<primary>q"])

        self.menu_btn = Gtk.MenuButton(
            icon_name="open-menu-symbolic",
            tooltip_text="Menu",
            menu_model=self.menu,
        )
        self.pack_end(self.menu_btn)

    def create_action(self, name: str, handler, accels=[]):
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", handler)
        data["app"].add_action(action)
        if accels != []:
            data["app"].set_accels_for_action(f"app.{name}", accels)

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
