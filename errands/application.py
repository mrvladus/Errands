# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
from subprocess import getoutput
from time import sleep

from gi.repository import Adw, Gio, GLib, Xdp  # type:ignore

from __main__ import APP_ID, VERSION
from errands.lib.data import UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.notifications import ErrandsNotificationsDaemon
from errands.lib.plugins import PluginsLoader
from errands.lib.utils import threaded
from errands.state import State
from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow
from errands.widgets.window import Window


class ErrandsApplication(Adw.Application):
    plugins_loader: PluginsLoader = None

    def __init__(self) -> None:
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.set_resource_base_path("/io/github/mrvladus/Errands/")
        State.application = self

    @threaded
    def check_reload(self) -> None:
        """Check if there is newer version installed"""

        TIMEOUT_SECONDS: int = 60
        portal: Xdp.Portal = Xdp.Portal()
        is_flatpak: bool = portal.running_under_flatpak()

        def __restart(*args):
            """Restart the app"""

            State.application.quit()

            if is_flatpak:
                cmd: str = "flatpak-spawn --host flatpak run io.github.mrvladus.List"
            else:
                cmd: str = "errands"
            os.system(cmd)
            exit()

        def __inner_check() -> bool:
            """Get installed version"""

            if not GSettings.get("run-in-background"):
                return True

            try:
                version: str = ""
                if is_flatpak:
                    out: str = getoutput(
                        "flatpak-spawn --host flatpak info io.github.mrvladus.List"
                    ).splitlines()
                    for line in out:
                        # TODO: Maybe use regex here
                        if VERSION[:3] in line:
                            version: str = line.split(" ")[-1]
                else:
                    version: str = (
                        getoutput(
                            "cat $(whereis -b errands | cut -d ' ' -f 2) | grep VERSION"
                        )
                        .split(" ")[-1]
                        .strip('"')
                    )
                if version:
                    # If installed version is different from running then show message
                    if version != VERSION:
                        restart_message = Adw.MessageDialog(
                            heading=_("Errands was updated"),
                            body=_("Restart is required"),
                            transient_for=State.main_window,
                        )
                        restart_message.add_response("restart", _("Restart"))
                        restart_message.set_default_response("restart")
                        restart_message.connect("response", __restart)
                        if State.main_window.get_visible():
                            GLib.idle_add(restart_message.present)
                        else:
                            GLib.idle_add(__restart)
                        return False
                    else:
                        return True
            except Exception:
                return True

        while True:
            sleep(TIMEOUT_SECONDS)
            if not __inner_check():
                break

    def run_in_background(self):
        """Create or remove autostart desktop file"""

        Log.debug("Application: Checking autostart")

        portal: Xdp.Portal = Xdp.Portal()

        # Request background
        portal.request_background(
            None,
            _("Errands need to run in the background for notifications"),
            ["errands", "--gapplication-service"],
            Xdp.BackgroundFlags.AUTOSTART,
            None,
            None,
            None,
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

        # Start notifications daemon
        ErrandsNotificationsDaemon()

        # Plugins
        # self.plugins_loader = PluginsLoader(self)

        # Notes window
        State.notes_window = NotesWindow()

        # Date and time window
        State.datetime_window = DateTimeWindow()

        # Main window
        State.main_window = Window(application=State.application)
        self.add_window(State.main_window)

        self.check_reload()

    def do_activate(self) -> None:
        Log.debug("Application: Activate")
        if not State.main_window:
            State.main_window = Window(application=State.application)
            self.add_window(State.main_window)
        State.main_window.present()
