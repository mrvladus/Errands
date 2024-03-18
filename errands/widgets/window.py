# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from __main__ import VERSION, APP_ID
import os
from uuid import uuid4
from icalendar import Calendar
from errands.lib.data import UserDataSQLite
from gi.repository import Gio, Adw, Gtk, GObject  # type:ignore

from errands.widgets.preferences import PreferencesWindow
from errands.lib.sync.sync import Sync
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.widgets.sidebar.sidebar import Sidebar


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    toast_overlay: Adw.ToastOverlay = Gtk.Template.Child()
    split_view: Adw.NavigationSplitView = Gtk.Template.Child()
    stack: Adw.ViewStack = Gtk.Template.Child()

    about_window: Adw.AboutWindow = None

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        self._create_actions()
        # Remember window state
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self.stack.set_visible_child_name("status")
        # Add Sidebar
        self.sidebar = Sidebar()
        self.split_view.set_sidebar(Adw.NavigationPage.new(self.sidebar, _("Sidebar")))
        # Setup sync
        Sync.window = self
        Sync.sync()

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
                        for i in UserDataSQLite.run_sql(
                            "SELECT name FROM lists", fetch=True
                        )
                    ]:
                        name = f"{name}_{uuid4()}"
                    # Create list
                    uid: str = UserDataSQLite.add_list(name)
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
                        UserDataSQLite.add_task(
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

    @Gtk.Template.Callback()
    def _on_add_list_clicked(self, btn: Gtk.Button):
        self.sidebar.add_list_btn.activate()
