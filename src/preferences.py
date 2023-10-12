# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
from gi.repository import Adw, Gtk
from .sync import Sync
from .utils import GSettings


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/preferences.ui")
class PreferencesWindow(Adw.PreferencesWindow):
    __gtype_name__ = "PreferencesWindow"

    system_theme: Gtk.CheckButton = Gtk.Template.Child()
    light_theme: Gtk.CheckButton = Gtk.Template.Child()
    dark_theme: Gtk.CheckButton = Gtk.Template.Child()
    expand_on_startup: Gtk.Switch = Gtk.Template.Child()
    sync_cal_name: Adw.EntryRow = Gtk.Template.Child()
    sync_providers: Adw.ComboRow = Gtk.Template.Child()
    sync_password: Adw.EntryRow = Gtk.Template.Child()
    sync_token: Adw.EntryRow = Gtk.Template.Child()
    sync_url: Adw.EntryRow = Gtk.Template.Child()
    sync_username: Adw.EntryRow = Gtk.Template.Child()
    test_connection_row: Adw.ActionRow = Gtk.Template.Child()

    selected_provider = 0

    def __init__(self, win: Adw.ApplicationWindow) -> None:
        super().__init__(transient_for=win)
        self.window = win
        # Setup theme
        theme: int = GSettings.get("theme")
        if theme == 0:
            self.system_theme.props.active = True
        elif theme == 1:
            self.light_theme.props.active = True
        elif theme == 4:
            self.dark_theme.props.active = True
        # Setup expand
        GSettings.bind("expand-on-startup", self.expand_on_startup, "active")
        # Setup sync
        GSettings.bind("sync-provider", self.sync_providers, "selected")
        GSettings.bind("sync-url", self.sync_url, "text")
        GSettings.bind("sync-username", self.sync_username, "text")
        GSettings.bind("sync-password", self.sync_password, "text")
        GSettings.bind("sync-cal-name", self.sync_cal_name, "text")
        self.setup_sync()

    def setup_sync(self):
        selected = self.sync_providers.props.selected
        self.sync_token.set_visible(selected == 3 and selected != 0)
        self.sync_url.set_visible(selected != 3 and selected != 0)
        self.sync_username.set_visible(selected != 3 and selected != 0)
        self.sync_password.set_visible(selected != 3 and selected != 0)
        self.sync_cal_name.set_visible(selected != 3 and selected != 0)
        self.test_connection_row.set_visible(selected > 0)

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_sync_provider_selected(self, *_) -> None:
        self.setup_sync()

    @Gtk.Template.Callback()
    def on_test_connection_btn_clicked(self, btn):
        res: bool = Sync.test_connection()
        msg = _("Connected") if res else _("Can't connect")  # pyright:ignore
        toast: Adw.Toast = Adw.Toast(title=msg, timeout=2)
        self.add_toast(toast)
        self.window.sync_btn.set_visible(res)

    @Gtk.Template.Callback()
    def on_theme_change(self, btn: Gtk.Button) -> None:
        id: str = btn.get_buildable_id()
        if id == "system_theme":
            theme = 0
        elif id == "light_theme":
            theme = 1
        elif id == "dark_theme":
            theme = 4
        Adw.StyleManager.get_default().set_color_scheme(theme)
        GSettings.set("theme", "i", theme)
