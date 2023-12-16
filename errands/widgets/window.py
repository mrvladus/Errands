# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __main__ import VERSION, APP_ID
from gi.repository import Gio, Adw, Gtk
from errands.widgets.lists import Lists
from errands.widgets.shortcuts_window import ShortcutsWindow
from errands.widgets.preferences import PreferencesWindow
from errands.utils.sync import Sync
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log


class Window(Adw.ApplicationWindow):
    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        # Remember window state
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self._create_actions()
        self._build_ui()
        self.present()
        # Setup sync
        Sync.init(window=self)
        Sync.sync()

    def _build_ui(self):
        self.props.width_request = 360
        self.props.height_request = 200
        self.set_default_size(1100, 700)
        # Split view
        self.split_view = Adw.NavigationSplitView(
            max_sidebar_width=240, min_sidebar_width=240, show_content=True
        )
        # Stack
        stack = Adw.ViewStack()
        self.split_view.set_content(Adw.NavigationPage.new(stack, "Tasks"))
        self.lists = Lists(self, stack)
        self.split_view.set_sidebar(Adw.NavigationPage.new(self.lists, "Lists"))
        # Status page
        status_page = Adw.StatusPage(
            title=_("No Task Lists"),  # type:ignore
            icon_name=APP_ID,
        )
        add_list_btn = Gtk.Button(
            label="Create List",
            halign="center",
            css_classes=["pill", "suggested-action"],
        )
        add_list_btn.connect("clicked", self.lists.on_add_btn_clicked)
        box = Gtk.Box(orientation="vertical", vexpand=True, valign="center")
        box.append(status_page)
        box.append(add_list_btn)
        status_toolbar_view = Adw.ToolbarView(content=box)
        status_toolbar_view.add_top_bar(Adw.HeaderBar(show_title=False))
        stack.add_titled(child=status_toolbar_view, name="status", title="Status")

        # Toast overlay
        self.toast_overlay = Adw.ToastOverlay(child=self.split_view)
        # Breakpoints
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
                application_name=_("Errands"),  # type:ignore
                copyright="© 2023 Vlad Krupinskii",
                website="https://github.com/mrvladus/Errands",
                issue_url="https://github.com/mrvladus/Errands/issues",
                license_type=Gtk.License.MIT_X11,
                translator_credits=_("translator-credits"),  # type:ignore
                modal=True,
            )
            about.show()

        def _sync(*args):
            Sync.sync()
            if GSettings.get("sync-provider") == 0:
                self.add_toast(_("Sync is disabled"))  # type:ignore

        _create_action(
            "preferences",
            lambda *_: PreferencesWindow(self).show(),
            ["<primary>comma"],
        )
        _create_action(
            "shortcuts", lambda *_: ShortcutsWindow(self), ["<primary>question"]
        )
        _create_action("about", _about)
        _create_action("sync", _sync, ["<primary>s"])
        _create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
