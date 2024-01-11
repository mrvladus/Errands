# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.goa import get_goa_credentials
from gi.repository import Adw, Gtk
from errands.utils.sync import Sync
from errands.utils.gsettings import GSettings


class PreferencesWindow(Adw.PreferencesWindow):
    selected_provider = 0

    def __init__(self, win: Adw.ApplicationWindow) -> None:
        super().__init__()
        self.window = win
        self._build_ui()
        self._setup_sync()

    def _build_ui(self) -> None:
        self.set_transient_for(self.window)
        self.set_search_enabled(False)

        # Theme group
        theme_group = Adw.PreferencesGroup(
            title=_("Application Theme"),
        )
        # System theme
        self.theme_system_btn = Gtk.CheckButton(active=GSettings.get("theme") == 0)
        self.theme_system_btn.connect("toggled", self.on_theme_change, 0)
        theme_system_row = Adw.ActionRow(
            title=_("System"),
            icon_name="errands-theme-system-symbolic",
        )
        theme_system_row.add_suffix(self.theme_system_btn)
        theme_system_row.set_activatable_widget(self.theme_system_btn)
        theme_group.add(theme_system_row)
        # Light theme
        self.theme_light_btn = Gtk.CheckButton(
            group=self.theme_system_btn, active=GSettings.get("theme") == 1
        )
        self.theme_light_btn.connect("toggled", self.on_theme_change, 1)
        theme_light_row = Adw.ActionRow(
            title=_("Light"),
            icon_name="errands-theme-light-symbolic",
        )
        theme_light_row.add_suffix(self.theme_light_btn)
        theme_light_row.set_activatable_widget(self.theme_light_btn)
        theme_group.add(theme_light_row)
        # Dark theme
        self.theme_dark_btn = Gtk.CheckButton(
            group=self.theme_system_btn, active=GSettings.get("theme") == 4
        )
        self.theme_dark_btn.connect("toggled", self.on_theme_change, 4)
        theme_dark_row = Adw.ActionRow(
            title=_("Dark"),
            icon_name="errands-theme-dark-symbolic",
        )
        theme_dark_row.add_suffix(self.theme_dark_btn)
        theme_dark_row.set_activatable_widget(self.theme_dark_btn)
        theme_group.add(theme_dark_row)

        # Sync group
        sync_group = Adw.PreferencesGroup(
            title=_("Sync"),
        )
        # Provider
        model = Gtk.StringList.new([_("Disabled"), "Nextcloud", "CalDAV"])
        self.sync_providers = Adw.ComboRow(
            title=_("Sync Provider"),
            model=model,
            icon_name="errands-sync-symbolic",
        )
        GSettings.bind("sync-provider", self.sync_providers, "selected")
        self.sync_providers.connect("notify::selected", self.on_sync_provider_selected)
        sync_group.add(self.sync_providers)
        # URL
        self.sync_url = Adw.EntryRow(
            title=_("Server URL"),
        )
        GSettings.bind("sync-url", self.sync_url, "text")
        sync_group.add(self.sync_url)
        # Username
        self.sync_username = Adw.EntryRow(
            title=_("Username"),
        )
        GSettings.bind("sync-username", self.sync_username, "text")
        sync_group.add(self.sync_username)
        # Password
        self.sync_password = Adw.PasswordEntryRow(
            title=_("Password"),
        )
        self.sync_password.connect("changed", self.on_sync_pass_changed)
        sync_group.add(self.sync_password)
        # Test connection
        test_btn = Gtk.Button(
            label=_("Test"),
            valign="center",
        )
        test_btn.connect("clicked", self.on_test_connection_btn_clicked)
        self.test_connection_row = Adw.ActionRow(
            title=_("Test Connection"),
        )
        self.test_connection_row.add_suffix(test_btn)
        self.test_connection_row.set_activatable_widget(test_btn)
        sync_group.add(self.test_connection_row)

        # Details group
        details_group = Adw.PreferencesGroup(
            title=_("Details Panel"),
        )
        # Left Sidebar
        self.left_sidebar_btn = Gtk.CheckButton(
            active=not GSettings.get("right-sidebar")
        )
        self.left_sidebar_btn.connect("toggled", self.on_details_change, False)
        left_sidebar_row = Adw.ActionRow(
            title=_("Left Sidebar"),
            icon_name="sidebar-show-symbolic",
        )
        left_sidebar_row.add_suffix(self.left_sidebar_btn)
        left_sidebar_row.set_activatable_widget(self.left_sidebar_btn)
        details_group.add(left_sidebar_row)
        # Right Sidebar
        self.right_sidebar_btn = Gtk.CheckButton(
            group=self.left_sidebar_btn, active=GSettings.get("right-sidebar")
        )
        self.right_sidebar_btn.connect("toggled", self.on_details_change, True)
        right_sidebar_row = Adw.ActionRow(
            title=_("Right Sidebar"),
            icon_name="sidebar-show-right-symbolic",
        )
        right_sidebar_row.add_suffix(self.right_sidebar_btn)
        right_sidebar_row.set_activatable_widget(self.right_sidebar_btn)
        details_group.add(right_sidebar_row)

        # Primary action group
        primary_action_group = Adw.PreferencesGroup(
            title=_("Primary Action"),
        )
        # Open Details Panel
        self.open_details_panel_btn = Gtk.CheckButton(
            active=not GSettings.get("primary-action-show-sub-tasks")
        )
        self.open_details_panel_btn.connect(
            "toggled",
            lambda *_: GSettings.set("primary-action-show-sub-tasks", "b", False),
        )
        open_details_panel_row = Adw.ActionRow(
            title=_("Open Details Panel"),
            icon_name="errands-info-symbolic",
        )
        open_details_panel_row.add_suffix(self.open_details_panel_btn)
        open_details_panel_row.set_activatable_widget(self.open_details_panel_btn)
        primary_action_group.add(open_details_panel_row)
        # Show Sub-Tasks Row
        self.show_sub_tasks_btn = Gtk.CheckButton(
            group=self.open_details_panel_btn,
            active=GSettings.get("primary-action-show-sub-tasks"),
        )
        self.show_sub_tasks_btn.connect(
            "toggled",
            lambda *_: GSettings.set("primary-action-show-sub-tasks", "b", True),
        )
        show_sub_tasks_row = Adw.ActionRow(
            title=_("Show Sub-Tasks"),
            icon_name="view-list-bullet-symbolic",
        )
        show_sub_tasks_row.add_suffix(self.show_sub_tasks_btn)
        show_sub_tasks_row.set_activatable_widget(self.show_sub_tasks_btn)
        primary_action_group.add(show_sub_tasks_row)

        # Page
        page = Adw.PreferencesPage()
        page.add(theme_group)
        page.add(sync_group)
        page.add(details_group)
        page.add(primary_action_group)
        self.add(page)

    def _setup_sync(self) -> None:
        selected = self.sync_providers.props.selected
        self.sync_url.set_visible(0 < selected < 3)
        self.sync_username.set_visible(0 < selected < 3)
        self.sync_password.set_visible(0 < selected < 3)
        self.test_connection_row.set_visible(selected > 0)

        if self.sync_password.props.visible:
            account = self.sync_providers.props.selected_item.props.string
            password = GSettings.get_secret(account)
            with self.sync_password.freeze_notify():
                self.sync_password.props.text = password if password else ""

        # Fill out forms from Gnome Online Accounts if needed
        acc_name: str = self.sync_providers.props.selected_item.props.string
        data: dict[str, str] | None = get_goa_credentials(acc_name)
        if data:
            if not GSettings.get("sync-url"):
                self.sync_url.set_text(data["url"])
            if not GSettings.get("sync-username"):
                self.sync_username.set_text(data["username"])
            if not GSettings.get_secret(acc_name):
                self.sync_password.set_text(data["password"])

    def on_sync_provider_selected(self, *_) -> None:
        self._setup_sync()

    def on_sync_pass_changed(self, _entry) -> None:
        if 0 < self.sync_providers.props.selected < 3:
            account = self.sync_providers.props.selected_item.props.string
            GSettings.set_secret(account, self.sync_password.props.text)

    def on_test_connection_btn_clicked(self, _btn) -> None:
        res: bool = Sync.test_connection()
        msg: str = _("Connected") if res else _("Can't connect")
        toast: Adw.Toast = Adw.Toast(title=msg, timeout=2)
        self.add_toast(toast)

    def on_theme_change(self, btn: Gtk.Button, theme: int) -> None:
        Adw.StyleManager.get_default().set_color_scheme(theme)
        GSettings.set("theme", "i", theme)

    def on_details_change(self, btn: Gtk.Button, right: bool) -> None:
        self.window.update_details(right)
        GSettings.set("right-sidebar", "b", right)
