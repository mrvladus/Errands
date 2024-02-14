from __main__ import APP_ID, PROFILE, IS_FLATPAK
import os
from gi.repository import Adw, Gio, GLib  # type:ignore
from errands.lib.gsettings import GSettings
from errands.widgets.window import Window
from errands.lib.plugins import PluginsLoader
from errands.lib.logging import Log
from errands.lib.data import UserData


class ErrandsApplication(Adw.Application):

    # Public elements
    window: Window
    plugins_loader: PluginsLoader = None

    def __init__(self) -> None:
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.set_resource_base_path("/io/github/mrvladus/Errands/")

    def run_in_background(self):
        """Create or remove autostart desktop file"""

        Log.debug("Application: Checking autostart")

        # Get or create autostart dir
        autostart_dir: str = os.path.join(GLib.get_home_dir(), ".config", "autostart")
        if not os.path.exists(autostart_dir):
            os.mkdir(autostart_dir)

        # Check if running in flatpak
        if IS_FLATPAK:
            autostart_file_content = f"""[Desktop Entry]
Type=Application
Name={APP_ID}
Exec=flatpak run --command=errands {APP_ID} --gapplication-service
X-Flatpak={APP_ID}"""
        else:
            autostart_file_content = f"""[Desktop Entry]
Type=Application
Name={APP_ID}
Exec=errands --gapplication-service"""

        # Create or remove autostart file
        file_path: str = os.path.join(autostart_dir, f"{APP_ID}.desktop")
        if GSettings.get("run-in-background") and not os.path.exists(file_path):
            with open(file_path, "w") as f:
                f.write(autostart_file_content)
        else:
            if os.path.exists(file_path):
                os.remove(file_path)

    def run_tests_suite(self):
        if PROFILE == "development":
            from errands.tests.tests import run_tests

            run_tests()

    def do_startup(self) -> None:
        # Logging
        Log.init()
        Log.debug("Application: Startup")

        # User database
        UserData.init()

        # GSettings
        GSettings.init()

        # Background
        self.run_in_background()

        # Startup
        Adw.Application.do_startup(self)

    def do_activate(self) -> None:
        Log.debug("Application: Activate")
        self.plugins_loader = PluginsLoader(self)
        self.window = Window(application=self)
        self.add_window(self.window)
        self.run_tests_suite()
        self.window.present()
