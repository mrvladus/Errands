from __main__ import APP_ID, PROFILE
from gi.repository import Adw, Gio, Xdp  # type:ignore
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

    def do_startup(self) -> None:
        Log.init()
        UserData.init()
        Adw.Application.do_startup(self)
        portal = Xdp.Portal.new()
        portal.request_background(
            None,
            "Errands need to run in the background",
            ["errands", "--gapplication-service"],
            Xdp.BackgroundFlags.AUTOSTART,
            None,
            None,
            None,
        )

    def do_activate(self) -> None:
        # self.plugins_loader = PluginsLoader(self)
        self.window = Window(application=self)
        # self.run_tests_suite()

    def run_tests_suite(self):
        if PROFILE == "development":
            from errands.tests.tests import run_tests

            run_tests()
