# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __main__ import VERSION, APP_ID
from errands.widgets.components import Box, Button
from errands.widgets.details import Details
from errands.widgets.trash import Trash
from gi.repository import Gio, Adw, Gtk
from errands.widgets.lists import Lists
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
        # Split view inner
        self.split_view_inner = Adw.OverlaySplitView(
            sidebar_position="start",
            min_sidebar_width=360,
            max_sidebar_width=360,
        )
        # Split view outer
        self.split_view = Adw.NavigationSplitView(
            max_sidebar_width=240, min_sidebar_width=240, show_content=True
        )
        # Stack
        self.stack = Adw.ViewStack()
        # Trash
        self.trash = Trash(self)
        self.stack.add_titled(self.trash, name="trash", title="Trash")
        # Details
        self.details = Details(self)
        GSettings.bind("sidebar-open", self.split_view_inner, "show-sidebar")
        self.split_view_inner.set_sidebar(self.details)
        self.split_view_inner.set_content(self.stack)
        # Lists
        self.lists = Lists(self, self.stack)
        self.split_view.set_sidebar(Adw.NavigationPage.new(self.lists, "Lists"))
        self.split_view.set_content(
            Adw.NavigationPage.new(self.split_view_inner, "Tasks")
        )
        # Status page Toolbar View
        status_toolbar_view = Adw.ToolbarView(
            content=Box(
                children=[
                    Adw.StatusPage(
                        title=_("No Task Lists"),  # type:ignore
                        icon_name=APP_ID,
                    ),
                    Button(
                        label="Create List",
                        on_click=lambda *_: self.lists.add_list_btn.activate(),
                        halign="center",
                        css_classes=["pill", "suggested-action"],
                    ),
                ],
                orientation="vertical",
                vexpand=True,
                valign="center",
            )
        )
        status_toolbar_view.add_top_bar(Adw.HeaderBar(show_title=False))
        self.stack.add_titled(
            child=status_toolbar_view,
            name="status",
            title=_("No Task Lists"),  # type:ignore
        )
        self.stack.set_visible_child_name("status")

        # Toast overlay
        self.toast_overlay = Adw.ToastOverlay(child=self.split_view)
        # Breakpoints
        bp = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 990px"))
        bp.add_setter(self.split_view, "collapsed", True)
        bp.add_setter(self.split_view, "show-content", True)
        bp1 = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 720px"))
        bp1.add_setter(self.split_view_inner, "collapsed", True)
        bp1.add_setter(self.split_view, "collapsed", True)
        bp1.add_setter(self.split_view, "show-content", True)
        self.add_breakpoint(bp)
        self.add_breakpoint(bp1)

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
        _create_action("about", _about)
        _create_action("sync", _sync, ["<primary>s"])
        _create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
