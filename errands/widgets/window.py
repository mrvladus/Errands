# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.widgets.lists import Lists
from errands.widgets.shortcuts_window import ShortcutsWindow
from gi.repository import Gio, Adw, Gtk
from __main__ import VERSION, APP_ID
from errands.widgets.preferences import PreferencesWindow
from errands.utils.sync import Sync
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log


class Window(Adw.ApplicationWindow):
    startup = True

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        # Remember window state
        Log.debug("Getting window settings")
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Log.debug("Setting theme")
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self._create_actions()
        self.build_ui()
        Log.debug("Present window")
        self.present()
        Sync.window = self
        self.startup = False

    def build_ui(self):
        self.props.width_request = 360
        self.props.height_request = 200
        # Stack
        stack = Adw.ViewStack()
        # Split view
        self.split_view = Adw.NavigationSplitView(
            max_sidebar_width=240, min_sidebar_width=240, show_content=True
        )
        self.split_view.set_sidebar(Adw.NavigationPage.new(Lists(self, stack), "Lists"))
        self.split_view.set_content(Adw.NavigationPage.new(stack, "Tasks"))
        # Toast overlay
        self.toast_overlay = Adw.ToastOverlay(child=self.split_view)

        bp = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 1080px"))
        bp.add_setter(self.split_view, "collapsed", True)
        bp.add_setter(self.split_view, "show-content", True)
        self.add_breakpoint(bp)

        self.set_content(self.toast_overlay)

    def add_toast(self, text: str) -> None:
        self.toast_overlay.add_toast(Adw.Toast.new(title=text))

    def _create_actions(self) -> None:
        """
        Create actions for main menu
        """
        Log.debug("Creating actions")

        def _create_action(name: str, callback: callable, shortcuts=None) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            if shortcuts:
                self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
            self.props.application.add_action(action)

        def _about(*args) -> None:
            """
            Show about window
            """
            about = Adw.AboutWindow(
                transient_for=self,
                version=VERSION,
                application_icon=APP_ID,
                application_name="Errands",
                copyright="© 2023 Vlad Krupinski",
                website="https://github.com/mrvladus/Errands",
                issue_url="https://github.com/mrvladus/Errands/issues",
                license_type=7,
                translator_credits=_("translator-credits"),  # type:ignore
                modal=True,
            )
            about.show()

        def _shortcuts(*_) -> None:
            """
            Show shortcuts window
            """

            ShortcutsWindow(self)

        _create_action(
            "preferences",
            lambda *_: PreferencesWindow(self).show(),
            ["<primary>comma"],
        )
        _create_action("shortcuts", _shortcuts, ["<primary>question"])
        _create_action("about", _about)
        _create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
