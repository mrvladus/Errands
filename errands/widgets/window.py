# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

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
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.sidebar import Sidebar
from errands.widgets.tags.tags import Tags
from errands.widgets.today.today import Today
from errands.widgets.trash.trash import Trash


class Window(Adw.ApplicationWindow):
    about_window: Adw.AboutWindow = None

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        Log.debug("Main Window: Load")
        self.__build_ui()
        self._create_actions()
        # Remember window state
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self.connect("realize", self.__finish_load)

    def __build_ui(self) -> None:
        self.set_title(_("Errands"))
        self.set_hide_on_close(True)
        self.props.width_request = 360
        self.props.height_request = 200

        # View Stack
        self.view_stack: Adw.ViewStack = Adw.ViewStack()

        # Split View
        self.split_view: Adw.NavigationSplitView = Adw.NavigationSplitView(
            show_content=True,
            max_sidebar_width=300,
            min_sidebar_width=200,
            sidebar=Adw.NavigationPage(child=Sidebar(), title=_("Sidebar")),
            content=Adw.NavigationPage(
                child=self.view_stack,
                title=_("Content"),
                width_request=360,
            ),
        )

        self.view_stack.add_titled(
            child=Today(),
            name="errands_today_page",
            title=_("Today"),
        )
        self.view_stack.add_titled(
            child=Tags(),
            name="errands_tags_page",
            title=_("Tags"),
        )
        self.view_stack.add_titled(
            child=Trash(),
            name="errands_trash_page",
            title=_("Trash"),
        )

        # Status Page
        self.view_stack.add_titled(
            child=ErrandsToolbarView(
                top_bars=[Adw.HeaderBar(show_title=False)],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    hexpand=True,
                    valign=Gtk.Align.CENTER,
                    children=[
                        Adw.StatusPage(
                            title=_("No Task Lists"),
                            description=_("Create new or import existing one"),
                            icon_name="io.github.mrvladus.List",
                        ),
                        ErrandsButton(
                            label=_("Create List"),
                            css_classes=["pill", "suggested-action"],
                            halign=Gtk.Align.CENTER,
                            on_click=lambda btn: State.sidebar.add_list_btn.activate(),
                        ),
                    ],
                ),
            ),
            name="errands_status_page",
            title=_("Create new List"),
        )

        # Toast Overlay
        self.toast_overlay: Adw.ToastOverlay = Adw.ToastOverlay(child=self.split_view)
        self.set_content(self.toast_overlay)

        # Breakpoints
        bp = Adw.Breakpoint(condition=Adw.BreakpointCondition.parse("max-width: 600px"))
        bp.add_setter(self.split_view, "collapsed", True)
        self.add_breakpoint(bp)

    def __finish_load(self, *_):
        State.view_stack = self.view_stack
        State.split_view = self.split_view
        State.view_stack.set_visible_child_name("errands_status_page")
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
                self.about_window = Adw.AboutDialog(
                    version=VERSION,
                    application_icon=APP_ID,
                    application_name=_("Errands"),
                    copyright="© 2023 Vlad Krupinskii",
                    website="https://github.com/mrvladus/Errands",
                    issue_url="https://github.com/mrvladus/Errands/issues",
                    license_type=Gtk.License.MIT_X11,
                    translator_credits=_("translator-credits"),
                )
            self.about_window.present(self)

        def _sync(*args):
            Sync.sync()
            if GSettings.get("sync-provider") == 0:
                self.add_toast(_("Sync is disabled"))

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
                            color=str(todo.get("x-errands-color", "")),
                            completed=str(todo.get("status", "")) == "COMPLETED",
                            expanded=bool(todo.get("x-errands-expanded", False)),
                            notes=str(todo.get("description", "")),
                            parent=str(todo.get("related-to", "")),
                            percent_complete=int(todo.get("percent-complete", 0)),
                            priority=int(todo.get("priority", 0)),
                            text=str(todo.get("summary", "")),
                            toolbar_shown=bool(
                                todo.get("x-errands-toolbar-shown", False)
                            ),
                            uid=str(todo.get("uid", "")),
                            list_uid=new_list.uid,
                        )

                        # Set tags
                        if (tags := todo.get("categories", "")) != "":
                            task.tags = [
                                i.to_ical().decode("utf-8")
                                for i in (tags if isinstance(tags, list) else tags.cats)
                            ]
                        else:
                            task.tags = []

                        # Set dates

                        if todo.get("due", "") != "":
                            task.due_date = (
                                todo.get("due", "").to_ical().decode("utf-8")
                            )
                            if task.due_date and "T" not in task.due_date:
                                task.due_date += "T000000"
                        else:
                            task.due_date = ""

                        if todo.get("dtstart", "") != "":
                            task.start_date = (
                                todo.get("dtstart", "").to_ical().decode("utf-8")
                            )
                            if task.start_date and "T" not in task.start_date:
                                task.start_date += "T000000"
                        else:
                            task.start_date = ""

                        if todo.get("dtstamp", "") != "":
                            task.created_at = (
                                todo.get("dtstamp", "").to_ical().decode("utf-8")
                            )
                        else:
                            task.created_at = ""

                        if todo.get("last-modified", "") != "":
                            task.changed_at = (
                                todo.get("last-modified", "").to_ical().decode("utf-8")
                            )
                        else:
                            task.changed_at = ""

                        UserData.add_task(**asdict(task))

                State.sidebar.add_task_list(new_list)
                self.add_toast(_("Imported"))
                Sync.sync()

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(default_filter=filter)
            dialog.open(self, None, _confirm)

        self._create_action(
            "preferences",
            lambda *_: PreferencesWindow().present(self),
            ["<primary>comma"],
        )
        self._create_action("about", _about)
        self._create_action("import", _import, ["<primary>i"])
        self._create_action("sync", _sync, ["<primary>f"])
        self._create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
