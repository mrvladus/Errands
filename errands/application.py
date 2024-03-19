from gi.repository import Adw, Gio, Xdp, GObject  # type:ignore
from errands.lib.gsettings import GSettings
from errands.widgets.window import Window
from errands.lib.plugins import PluginsLoader
from errands.lib.logging import Log
from errands.lib.data import UserData


class ErrandsApplication(Adw.Application):

    # Public elements
    window: Window
    plugins_loader: PluginsLoader = None

    def __init__(self, APP_ID) -> None:
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.set_resource_base_path("/io/github/mrvladus/Errands/")

    def run_in_background(self):
        """Create or remove autostart desktop file"""

        Log.debug("Application: Checking autostart")

        portal: Xdp.Portal = Xdp.Portal()
        portal.request_background(
            None,
            _("Errands need to run in the background for notifications"),
            ["errands", "--gapplication-service"],
            Xdp.BackgroundFlags.AUTOSTART,
            None,
            None,
            None,
        )

    def run_tests_suite(self):
        pass
        # if PROFILE == "development":
        #     from errands.tests.tests import run_tests

        #     run_tests()

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

        # Main window
        self.window = Window(application=self)
        self.add_window(self.window)

    def do_activate(self) -> None:
        Log.debug("Application: Activate")
        self.window.present()
        # self.run_tests_suite()
