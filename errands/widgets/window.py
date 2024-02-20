# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from __main__ import VERSION, APP_ID
from uuid import uuid4
from icalendar import Calendar
from errands.lib.data import UserData
from errands.widgets.components import Box, Button

# from errands.plugins.secret_notes import SecretNotesWindow
from errands.widgets.trash import Trash
from gi.repository import Gio, Adw, Gtk  # type:ignore
from errands.widgets.sidebar import Sidebar
from errands.widgets.preferences import PreferencesWindow
from errands.lib.sync.sync import Sync
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log

WINDOW: Window = None


class Window(Adw.ApplicationWindow):
    about_window: Adw.AboutWindow = None

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        global WINDOW
        WINDOW = self
        self._create_actions()
        self._build_ui()
        # Setup sync
        Sync.window = self
        Sync.sync()

    def _build_ui(self):
        self.set_title(_("Errands"))
        self.props.width_request = 360
        self.props.height_request = 200
        # Remember window state
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))

        # Split View
        self.split_view = Adw.NavigationSplitView(
            max_sidebar_width=300,
            min_sidebar_width=240,
            show_content=True,
            sidebar_width_fraction=0.25,
        )

        # Stack
        self.stack = Adw.ViewStack()

        # Status page Toolbar View
        status_toolbar_view = Adw.ToolbarView(
            content=Box(
                children=[
                    Adw.StatusPage(title=_("No Task Lists"), icon_name=APP_ID),
                    Button(
                        label=_("Create List"),
                        on_click=lambda *_: self.sidebar.header_bar.add_list_btn.activate(),
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
            title=_("No Task Lists"),
        )
        self.stack.set_visible_child_name("status")

        # Sidebar
        self.sidebar = Sidebar(self)
        self.split_view.set_sidebar(Adw.NavigationPage.new(self.sidebar, _("Lists")))
        self.split_view.set_content(Adw.NavigationPage.new(self.stack, _("Tasks")))

        # Toast overlay
        self.toast_overlay = Adw.ToastOverlay(child=self.split_view)

        # Breakpoints
        bp = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 980px"))
        bp.add_setter(self.split_view, "collapsed", True)
        bp.add_setter(self.split_view, "show-content", True)
        self.add_breakpoint(bp)

        self.set_content(self.toast_overlay)

    def add_toast(self, text: str) -> None:
        self.toast_overlay.add_toast(Adw.Toast.new(title=text))

    def _create_action(self, name: str, callback: callable, shortcuts=None) -> None:
        action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        if shortcuts:
            self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
        self.props.application.add_action(action)

    def _create_actions(self) -> None:
        """
        Create actions for main menu
        """
        Log.debug("Creating actions")

        def _about(*args) -> None:
            """
            Show about window
            """
            if not self.about_window:
                self.about_window = Adw.AboutWindow(
                    transient_for=self,
                    version=VERSION,
                    application_icon=APP_ID,
                    application_name=_("Errands"),
                    copyright="© 2023 Vlad Krupinskii",
                    website="https://github.com/mrvladus/Errands",
                    issue_url="https://github.com/mrvladus/Errands/issues",
                    license_type=Gtk.License.MIT_X11,
                    translator_credits=_("translator-credits"),
                    modal=True,
                    hide_on_close=True,
                )
            self.about_window.present()

        def _sync(*args):
            Sync.sync()
            if GSettings.get("sync-provider") == 0:
                self.add_toast(_("Sync is disabled"))

        def _import(*args) -> None:
            def _confirm(dialog: Gtk.FileDialog, res) -> None:
                try:
                    file: Gio.File = dialog.open_finish(res)
                except:
                    Log.debug("Lists: Import cancelled")
                    return
                with open(file.get_path(), "r") as f:
                    calendar: Calendar = Calendar.from_ical(f.read())
                    # List name
                    name = calendar.get(
                        "X-WR-CALNAME", file.get_basename().rstrip(".ics")
                    )
                    if name in [
                        i[0]
                        for i in UserData.run_sql("SELECT name FROM lists", fetch=True)
                    ]:
                        name = f"{name}_{uuid4()}"
                    # Create list
                    uid: str = UserData.add_list(name)
                    # Add tasks
                    for todo in calendar.walk("VTODO"):
                        # Tags
                        if (tags := todo.get("CATEGORIES", "")) != "":
                            tags = ",".join(
                                [
                                    i.to_ical().decode("utf-8")
                                    for i in (
                                        tags if isinstance(tags, list) else tags.cats
                                    )
                                ]
                            )
                        # Start
                        if (start := todo.get("DTSTART", "")) != "":
                            start = (
                                todo.get("DTSTART", "")
                                .to_ical()
                                .decode("utf-8")
                                .strip("Z")
                            )
                        else:
                            start = ""
                        # End
                        if (end := todo.get("DUE", todo.get("DTEND", ""))) != "":
                            end = (
                                todo.get("DUE", todo.get("DTEND", ""))
                                .to_ical()
                                .decode("utf-8")
                                .strip("Z")
                            )
                        else:
                            end = ""
                        UserData.add_task(
                            color=todo.get("X-ERRANDS-COLOR", ""),
                            completed=str(todo.get("STATUS", "")) == "COMPLETED",
                            end_date=end,
                            list_uid=uid,
                            notes=str(todo.get("DESCRIPTION", "")),
                            parent=str(todo.get("RELATED-TO", "")),
                            percent_complete=int(todo.get("PERCENT-COMPLETE", 0)),
                            priority=int(todo.get("PRIORITY", 0)),
                            start_date=start,
                            tags=tags,
                            text=str(todo.get("SUMMARY", "")),
                            uid=todo.get("UID", None),
                        )
                self.sidebar.task_lists.update_ui()
                self.add_toast(_("Imported"))
                Sync.sync()

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(default_filter=filter)
            dialog.open(self, None, _confirm)

        self._create_action(
            "preferences",
            lambda *_: PreferencesWindow(self).show(),
            ["<primary>comma"],
        )
        self._create_action("about", _about)
        self._create_action("import", _import)
        self._create_action("sync", _sync, ["<primary>f"])
        self._create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
