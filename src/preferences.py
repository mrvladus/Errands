from gi.repository import Adw, Gtk, GLib
from .main import gsettings


@Gtk.Template(resource_path="/io/github/mrvladus/List/preferences.ui")
class PreferencesWindow(Adw.PreferencesWindow):
    __gtype_name__ = "PreferencesWindow"

    system_theme = Gtk.Template.Child()
    light_theme = Gtk.Template.Child()
    dark_theme = Gtk.Template.Child()
    tasks_expanded = Gtk.Template.Child()

    def __init__(self, win):
        super().__init__(transient_for=win)
        # Setup theme
        theme = gsettings.get_value("theme").unpack()
        if theme == 0:
            self.system_theme.props.active = True
        if theme == 1:
            self.light_theme.props.active = True
        if theme == 4:
            self.dark_theme.props.active = True
        # Setup tasks
        gsettings.bind("tasks-expanded", self.tasks_expanded, "active", 0)

    @Gtk.Template.Callback()
    def on_theme_change(self, btn):
        id = btn.get_buildable_id()
        if id == "system_theme":
            theme = 0
        elif id == "light_theme":
            theme = 1
        elif id == "dark_theme":
            theme = 4
        Adw.StyleManager.get_default().set_color_scheme(theme)
        gsettings.set_value("theme", GLib.Variant("i", theme))
