# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk

# Import modules
from errands.utils.sync import Sync
from errands.utils.gsettings import GSettings
from errands.utils.data import UserData, UserDataDict


class PreferencesWindow(Adw.PreferencesWindow):
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

    def build_ui(self):
        self.set_hide_on_close(True)
        self.set_search_enabled(False)
        # Theme group
        theme_group = Adw.PreferencesGroup(
            title=_("Application Theme"),  # type:ignore
        )
        # System theme
        theme_system_btn = Gtk.CheckButton()
        theme_system_btn.connect("toggled", self.on_theme_change, 0)
        theme_system_row = Adw.ActionRow(
            title=_("System"),  # type:ignore
            icon_name="applications-system-symbolic",
        )
        theme_system_row.add_suffix(theme_system_btn)
        theme_system_row.set_activatable_widget(theme_system_btn)
        theme_group.add(theme_system_row)
        # Light theme
        theme_light_btn = Gtk.CheckButton(group=theme_system_btn)
        theme_light_btn.connect("toggled", self.on_theme_change, 1)
        theme_light_row = Adw.ActionRow(
            title=_("Light"),  # type:ignore
            icon_name="display-brightness-symbolic",
        )
        theme_light_row.add_suffix(theme_light_btn)
        theme_light_row.set_activatable_widget(theme_light_btn)
        theme_group.add(theme_light_row)
        # Dark theme
        theme_dark_btn = Gtk.CheckButton(group=theme_system_btn)
        theme_light_btn.connect("toggled", self.on_theme_change, 4)
        theme_dark_row = Adw.ActionRow(
            title=_("Dark"),  # type:ignore
            icon_name="weather-clear-night-symbolic",
        )
        theme_dark_row.add_suffix(theme_dark_btn)
        theme_dark_row.set_activatable_widget(theme_dark_btn)
        theme_group.add(theme_dark_row)
        # Tasks group
        tasks_group = Adw.PreferencesGroup(
            title=_("Tasks"),  # type:ignore
        )
        tasks_expand_row = Adw.ActionRow(
            title=_("Expand Sub-Tasks on Startup"),  # type:ignore
        )
        self.expand_on_startup = Gtk.Switch(valign="center")
        tasks_expand_row.add_suffix(self.expand_on_startup)
        tasks_expand_row.set_activatable_widget(self.expand_on_startup)
        tasks_group.add(tasks_expand_row)
        # Sync group
        sync_group = Adw.PreferencesGroup(
            title=_("Sync"),  # type:ignore
        )
        # Provider
        model = Gtk.StringList([_("Disabled"), "Nextcloud", "CalDAV"])  # type:ignore
        self.sync_providers = Adw.ComboRow(
            title=_("Sync Provider"),  # type:ignore
            model=model,
        )
        self.sync_providers.connect("notify::selected", self.on_sync_provider_selected)
        # URL
        self.sync_url = Adw.EntryRow(
            title=_("Server URL"),  # type:ignore
        )
        # Username
        self.sync_username = Adw.EntryRow(
            title=_("Username"),  # type:ignore
        )
        # Password
        self.sync_url = Adw.PasswordEntryRow(
            title=_("Password"),  # type:ignore
        )
        # Test connection
        test_btn = Gtk.Button(
            label=_("Test"),  # type:ignore
            valign="center",
        )
        test_btn.connect("clicked", self.on_test_connection_btn_clicked)
        self.test_connection_row = Adw.ActionRow(
            title=_("Test Connection"),  # type:ignore
        )
        self.test_connection_row.add_suffix(test_btn)
        self.test_connection_row.set_activatable_widget(test_btn)
        # Page
        page = Adw.PreferencesPage()
        page.add(theme_group)
        page.add(tasks_group)
        page.add(sync_group)

    def setup_sync(self):
        selected = self.sync_providers.props.selected
        self.sync_url.set_visible(0 < selected < 3)
        self.sync_username.set_visible(0 < selected < 3)
        self.sync_password.set_visible(0 < selected < 3)
        self.sync_cal_name.set_visible(0 < selected < 3)
        self.sync_token.set_visible(0 < selected > 3)
        self.test_connection_row.set_visible(selected > 0)
        self.window.sync_btn.set_visible(selected > 0)

    def on_cal_name_changed(self, *args):
        data: UserDataDict = UserData.get()
        data["tasks"] = [task for task in data["tasks"] if not task["synced_caldav"]]
        UserData.set(data)
        Sync.init()
        Sync.sync(True)

    def on_sync_provider_selected(self, *_) -> None:
        self.setup_sync()

    def on_test_connection_btn_clicked(self, _btn):
        res: bool = Sync.test_connection()
        msg = _("Connected") if res else _("Can't connect")  # pyright:ignore
        toast: Adw.Toast = Adw.Toast(title=msg, timeout=2)
        self.add_toast(toast)
        self.window.sync_btn.set_visible(res)

    def on_theme_change(self, btn: Gtk.Button, theme: int) -> None:
        Adw.StyleManager.get_default().set_color_scheme(theme)
        GSettings.set("theme", "i", theme)
