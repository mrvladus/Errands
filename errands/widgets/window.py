# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os
from dataclasses import asdict
from uuid import uuid4

from gi.repository import Adw, Gio, Gtk  # type:ignore
from icalendar import Calendar

from __main__ import APP_ID, VERSION
from errands.lib.data import TaskData, TaskListData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.preferences import PreferencesWindow


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    view_stack: Adw.ViewStack = Gtk.Template.Child()
    split_view: Adw.NavigationSplitView = Gtk.Template.Child()
    toast_overlay: Adw.ToastOverlay = Gtk.Template.Child()

    about_window: Adw.AboutWindow = None

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        Log.debug("Main Window: Load")

        self._create_actions()
        # Remember window state
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self.connect("realize", self.__finish_load)

    def __finish_load(self, *_):
        State.view_stack = self.view_stack
        State.split_view = self.split_view
        State.view_stack.set_visible_child_name("status")
        State.sidebar.load_task_lists()
        State.trash_sidebar_row.update_ui()
        # Sync
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
                    application_name=_("Errands"),  # noqa: F821
                    copyright="© 2023 Vlad Krupinskii",
                    website="https://github.com/mrvladus/Errands",
                    issue_url="https://github.com/mrvladus/Errands/issues",
                    license_type=Gtk.License.MIT_X11,
                    translator_credits=_("translator-credits"),  # noqa: F821
                    modal=True,
                    hide_on_close=True,
                )
            self.about_window.present()

        def _sync(*args):
            Sync.sync()
            if GSettings.get("sync-provider") == 0:
                self.add_toast(_("Sync is disabled"))  # noqa: F821

        def _import(*args) -> None:
            def _confirm(dialog: Gtk.FileDialog, res) -> None:
                try:
                    file: Gio.File = dialog.open_finish(res)
                except Exception as e:
                    Log.debug(f"Lists: Import cancelled. {e}")
                    return
                with open(file.get_path(), "r") as f:
                    calendar: Calendar = Calendar.from_ical(f.read())
                    # List name
                    name = calendar.get(
                        "X-WR-CALNAME", file.get_basename().rstrip(".ics")
                    )
                    if name in [lst.name for lst in UserData.get_lists_as_dicts()]:
                        name = f"{name}_{uuid4()}"
                    # Create list
                    new_list: TaskListData = UserData.add_list(name=name)
                    # Add tasks
                    for todo in calendar.walk("VTODO"):
                        task: TaskData = TaskData(
                            color=str(
                                todo.icalendar_component.get("x-errands-color", "")
                            ),
                            completed=str(todo.icalendar_component.get("status", ""))
                            == "COMPLETED",
                            expanded=bool(
                                todo.icalendar_component.get(
                                    "x-errands-expanded", False
                                )
                            ),
                            notes=str(todo.icalendar_component.get("description", "")),
                            parent=str(todo.icalendar_component.get("related-to", "")),
                            percent_complete=int(
                                todo.icalendar_component.get("percent-complete", 0)
                            ),
                            priority=int(todo.icalendar_component.get("priority", 0)),
                            text=str(todo.icalendar_component.get("summary", "")),
                            toolbar_shown=bool(
                                todo.icalendar_component.get(
                                    "x-errands-toolbar-shown", False
                                )
                            ),
                            uid=str(todo.icalendar_component.get("uid", "")),
                            list_uid=new_list.uid,
                        )

                        # Set tags
                        if (
                            tags := todo.icalendar_component.get("categories", "")
                        ) != "":
                            task.tags = [
                                i.to_ical().decode("utf-8")
                                for i in (tags if isinstance(tags, list) else tags.cats)
                            ]
                        else:
                            task.tags = []

                        # Set dates

                        if todo.icalendar_component.get("due", "") != "":
                            task.due_date = (
                                todo.icalendar_component.get("due", "")
                                .to_ical()
                                .decode("utf-8")
                            )
                            if task.due_date and "T" not in task.due_date:
                                task.due_date += "T000000"
                        else:
                            task.due_date = ""

                        if todo.icalendar_component.get("dtstart", "") != "":
                            task.start_date = (
                                todo.icalendar_component.get("dtstart", "")
                                .to_ical()
                                .decode("utf-8")
                            )
                            if task.start_date and "T" not in task.start_date:
                                task.start_date += "T000000"
                        else:
                            task.start_date = ""

                        if todo.icalendar_component.get("dtstamp", "") != "":
                            task.created_at = (
                                todo.icalendar_component.get("dtstamp", "")
                                .to_ical()
                                .decode("utf-8")
                            )
                        else:
                            task.created_at = ""

                        if todo.icalendar_component.get("last-modified", "") != "":
                            task.changed_at = (
                                todo.icalendar_component.get("last-modified", "")
                                .to_ical()
                                .decode("utf-8")
                            )
                        else:
                            task.changed_at = ""

                        UserData.add_task(**asdict(task))

                State.sidebar.task_lists.update_ui()
                self.add_toast(_("Imported"))  # noqa: F821
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
        State.sidebar.add_list_btn.activate()
