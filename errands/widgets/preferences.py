# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk

# Import modules
from errands.utils.sync import Sync
from errands.utils.gsettings import GSettings
from errands.utils.data import UserData, UserDataDict


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
        self.sync_url.set_visible(0 < selected < 3)
        self.sync_username.set_visible(0 < selected < 3)
        self.sync_password.set_visible(0 < selected < 3)
        self.sync_cal_name.set_visible(0 < selected < 3)
        self.sync_token.set_visible(0 < selected > 3)
        self.test_connection_row.set_visible(selected > 0)
        self.window.sync_btn.set_visible(selected > 0)

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_cal_name_changed(self, *args):
        data: UserDataDict = UserData.get()
        data["tasks"] = [task for task in data["tasks"] if not task["synced_caldav"]]
        UserData.set(data)
        Sync.init()
        Sync.sync(True)

    @Gtk.Template.Callback()
    def on_sync_provider_selected(self, *_) -> None:
        self.setup_sync()

    @Gtk.Template.Callback()
    def on_test_connection_btn_clicked(self, _btn):
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
