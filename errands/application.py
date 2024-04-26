# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gio, Xdp  # type:ignore

from errands.lib.data import UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.notifications import ErrandsNotificationsDaemon
from errands.lib.plugins import PluginsLoader
from errands.state import State
from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow
from errands.widgets.window import Window


class ErrandsApplication(Adw.Application):
    plugins_loader: PluginsLoader = None

    def __init__(self, APP_ID) -> None:
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.set_resource_base_path("/io/github/mrvladus/Errands/")
        State.application = self

    def run_in_background(self):
        """Create or remove autostart desktop file"""

        Log.debug("Application: Checking autostart")

        portal: Xdp.Portal = Xdp.Portal()

        # Request background
        if GSettings.get("run-in-background"):
            portal.request_background(
                None,
                _("Errands need to run in the background for notifications"),
                ["errands", "--gapplication-service"],
                Xdp.BackgroundFlags.AUTOSTART,
                None,
                None,
                None,
            )

        # Run notifications daemon
        self.notification_daemon: ErrandsNotificationsDaemon = (
            ErrandsNotificationsDaemon()
        )

    def do_startup(self) -> None:
        Adw.Application.do_startup(self)

        # Logging
        Log.init()
        Log.debug("Application: Startup")

        # User database
        UserData.init()

        # GSettings
        GSettings.init()

        # Background
        self.run_in_background()

        # Plugins
        # self.plugins_loader = PluginsLoader(self)

        # Notes window
        State.notes_window = NotesWindow()

        # Date and time window
        State.datetime_window = DateTimeWindow()

        # Main window
        State.main_window = Window(application=State.application)
        self.add_window(State.main_window)

    def do_activate(self) -> None:
        Log.debug("Application: Activate")
        if not State.main_window:
            State.main_window = Window(application=State.application)
            self.add_window(State.main_window)
        State.main_window.present()
