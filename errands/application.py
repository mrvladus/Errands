from __main__ import APP_ID, PROFILE
from gi.repository import Adw, Gio  # type:ignore
from errands.widgets.window import Window
from errands.lib.plugins import PluginsLoader


class ErrandsApplication(Adw.Application):

    # Public elements
    window: Window

    def __init__(self) -> None:
        super().__init__(
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.set_resource_base_path("/io/github/mrvladus/Errands/")

    def do_activate(self) -> None:
        self.window = Window(application=self)
        plugins_loader = PluginsLoader(self)
        self.run_tests_suite()

    def run_tests_suite(self):
        if PROFILE == "development":
            from errands.tests.tests import run_tests

            run_tests()
