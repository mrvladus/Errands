# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.goa import get_goa_credentials
from errands.lib.gsettings import GSettings
from errands.lib.sync.sync import Sync
from caldav.lib import error
from requests import exceptions
from errands.widgets.shared.components.buttons import ErrandsButton


class PreferencesWindow(Adw.PreferencesDialog):
    selected_provider: int = 0

    def __init__(self) -> None:
        super().__init__()
        self._build_ui()
        self._setup_sync()

    def _build_ui(self) -> None:
        # Theme group
        theme_group: Adw.PreferencesGroup = Adw.PreferencesGroup(
            title=_("Application Theme"),
        )
        # System theme
        self.theme_system_btn: Gtk.CheckButton = Gtk.CheckButton(
            active=GSettings.get("theme") == 0
        )
        self.theme_system_btn.connect("toggled", self.on_theme_change, 0)
        theme_system_row: Adw.ActionRow = Adw.ActionRow(
            title=_("System"),
            icon_name="errands-theme-system-symbolic",
        )
        theme_system_row.add_suffix(self.theme_system_btn)
        theme_system_row.set_activatable_widget(self.theme_system_btn)
        theme_group.add(theme_system_row)
        # Light theme
        self.theme_light_btn: Gtk.CheckButton = Gtk.CheckButton(
            group=self.theme_system_btn, active=GSettings.get("theme") == 1
        )
        self.theme_light_btn.connect("toggled", self.on_theme_change, 1)
        theme_light_row: Adw.ActionRow = Adw.ActionRow(
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
        theme_dark_row: Adw.ActionRow = Adw.ActionRow(
            title=_("Dark"),
            icon_name="errands-theme-dark-symbolic",
        )
        theme_dark_row.add_suffix(self.theme_dark_btn)
        theme_dark_row.set_activatable_widget(self.theme_dark_btn)
        theme_group.add(theme_dark_row)

        # Task lists group
        task_list_group = Adw.PreferencesGroup(title=_("Task Lists"))

        # New task position
        new_task_position = Adw.ComboRow(
            title=_("Add new Tasks"),
            model=Gtk.StringList.new([_("At the Bottom"), _("At the Top")]),
            icon_name="errands-add-symbolic",
        )
        new_task_position.set_selected(
            int(GSettings.get("task-list-new-task-position-top"))
        )
        new_task_position.connect(
            "notify::selected",
            lambda row, *_: GSettings.set(
                "task-list-new-task-position-top", "b", bool(row.get_selected())
            ),
        )
        task_list_group.add(new_task_position)

        # Notifications
        notifications = Adw.SwitchRow(
            title=_("Show Notifications"),
            icon_name="errands-notification-symbolic",
        )
        GSettings.bind("notifications-enabled", notifications, "active")

        # Background
        background = Adw.SwitchRow(
            title=_("Run in Background"),
            subtitle=_("Hide the application window instead of closing it"),
            icon_name="errands-progressbar-symbolic",
        )
        GSettings.bind("run-in-background", background, "active")

        notifications_and_bg_group = Adw.PreferencesGroup(
            title=_("Notifications and Background")
        )
        notifications_and_bg_group.add(notifications)
        notifications_and_bg_group.add(background)

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
        self.sync_providers.connect("notify::selected", lambda *_: self._setup_sync())
        sync_group.add(self.sync_providers)
        # URL
        self.sync_url = Adw.EntryRow(
            title=_("Server URL"),
        )
        GSettings.bind("sync-url", self.sync_url, "text")
        self.sync_url.add_suffix(
            Gtk.MenuButton(
                valign=Gtk.Align.CENTER,
                icon_name="errands-info-symbolic",
                tooltip_text=_("Info"),
                css_classes=["flat"],
                popover=Gtk.Popover(
                    child=Gtk.Label(
                        label=_(
                            "URL needs to include protocol, like <b>http://</b> or <b>https://</b>. If you have problems with connection - try to change protocol first."
                        ),
                        use_markup=True,
                        lines=5,
                        wrap_mode=0,
                        wrap=True,
                        max_width_chars=20,
                        margin_bottom=6,
                        margin_top=6,
                        margin_end=3,
                        margin_start=3,
                    )
                ),
            )
        )
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
        test_btn = ErrandsButton(
            label=_("Test"),
            valign="center",
            on_click=self.on_test_connection_btn_clicked,
        )
        self.test_connection_row = Adw.ActionRow(
            title=_("Test Connection"),
        )
        self.test_connection_row.add_suffix(test_btn)
        self.test_connection_row.set_activatable_widget(test_btn)
        sync_group.add(self.test_connection_row)

        # Appearance Page
        appearance_page = Adw.PreferencesPage(
            title=_("General"), icon_name="errands-settings-symbolic"
        )
        appearance_page.add(theme_group)
        appearance_page.add(task_list_group)
        appearance_page.add(notifications_and_bg_group)
        self.add(appearance_page)

        # Sync Page
        sync_page = Adw.PreferencesPage(
            title=_("Sync"), icon_name="errands-sync-symbolic"
        )
        sync_page.add(sync_group)
        self.add(sync_page)

    def _setup_sync(self) -> None:
        selected: int = self.sync_providers.props.selected
        self.sync_url.set_visible(0 < selected < 3)
        self.sync_username.set_visible(0 < selected < 3)
        self.sync_password.set_visible(0 < selected < 3)
        self.test_connection_row.set_visible(selected > 0)

        if self.sync_password.props.visible:
            account: str = self.sync_providers.props.selected_item.props.string
            password: str = GSettings.get_secret(account)
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

    def on_sync_pass_changed(self, _entry) -> None:
        if 0 < self.sync_providers.props.selected < 3:
            account = self.sync_providers.props.selected_item.props.string
            GSettings.set_secret(account, self.sync_password.props.text)

    def on_test_connection_btn_clicked(self, _btn) -> None:
        res, err = Sync.test_connection()
        msg: str = _("Connected")
        if not res:
            match type(err):
                case error.AuthorizationError:
                    msg: str = _("Authorization failed")
                case exceptions.ConnectionError:
                    msg: str = _("Could not locate server. Check network and url.")
                case error.PropfindError:
                    msg: str = _("Can't connect")
                case _:  # NOTE: Also catches invalid credentials
                    msg: str = _("Can't connect. Check credentials")

        toast: Adw.Toast = Adw.Toast(title=msg, timeout=2)
        self.add_toast(toast)

    def on_theme_change(self, btn: Gtk.Button, theme: int) -> None:
        Adw.StyleManager.get_default().set_color_scheme(theme)
        GSettings.set("theme", "i", theme)
