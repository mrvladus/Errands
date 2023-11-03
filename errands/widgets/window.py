# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import json
from errands.widgets.lists_panel import ListsPanel
from errands.widgets.shortcuts_window import ShortcutsWindow
from errands.widgets.task_details import TaskDetails
from errands.widgets.tasks_list import TasksList
from errands.widgets.trash import TrashPanel
from gi.repository import Gio, Adw, Gtk, GLib, GObject
from __main__ import VERSION, APP_ID
from errands.widgets.preferences import PreferencesWindow
from errands.utils.sync import Sync
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.data import UserData
from errands.utils.functions import get_children

GObject.type_ensure(TrashPanel)
GObject.type_ensure(TaskDetails)
GObject.type_ensure(ListsPanel)
GObject.type_ensure(TasksList)


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    # - Template children - #
    about_window: Adw.AboutWindow = Gtk.Template.Child()
    export_dialog: Gtk.FileDialog = Gtk.Template.Child()
    import_dialog: Gtk.FileDialog = Gtk.Template.Child()
    toast_overlay: Adw.ToastOverlay = Gtk.Template.Child()

    lists_panel = Gtk.Template.Child()
    tasks_list = Gtk.Template.Child()

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
        Log.debug("Present window")
        self.present()

    def perform_startup(self) -> None:
        """
        Startup func. Call after window is presented.
        """
        Log.debug("Window startup")
        Sync.window = self
        self._create_actions()
        self.tasks_list.load_tasks()
        self.startup = False

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

        def _about(*_) -> None:
            """
            Show about window
            """

            self.about_window.props.version = VERSION
            self.about_window.props.application_icon = APP_ID
            self.about_window.show()

        def _export_tasks(*args) -> None:
            """
            Show export dialog
            """

            def _finish_export(_dial, res, _data) -> None:
                try:
                    file: Gio.File = self.export_dialog.save_finish(res)
                except GLib.GError:
                    Log.info("Export cancelled")
                    self.add_toast(_("Export Cancelled"))  # pyright:ignore
                    return
                try:
                    path: str = file.get_path()
                    with open(path, "w+") as f:
                        json.dump(UserData.get(), f, indent=4, ensure_ascii=False)
                    self.add_toast(_("Tasks Exported"))  # pyright:ignore
                    Log.info(f"Export tasks to: {path}")
                except:
                    self.add_toast(_("Error"))  # pyright:ignore
                    Log.info(f"Can't export tasks to: {path}")

            self.export_dialog.save(self, None, _finish_export, None)

        def _import_tasks(*args) -> None:
            """
            Show import dialog
            """

            def finish_import(_dial, res, _data) -> None:
                Log.info("Importing tasks")

                try:
                    file: Gio.File = self.import_dialog.open_finish(res)
                except GLib.GError:
                    Log.info("Import cancelled")
                    self.add_toast(_("Import Cancelled"))  # pyright:ignore
                    return

                with open(file.get_path(), "r") as f:
                    text: str = f.read()
                    try:
                        text = UserData.convert(json.loads(text))
                    except:
                        Log.error("Invalid file")
                        self.add_toast(_("Invalid File"))  # pyright:ignore
                        return
                    data: dict = UserData.get()
                    ids = [t["id"] for t in data["tasks"]]
                    for task in text["tasks"]:
                        if task["id"] not in ids:
                            data["tasks"].append(task)
                    data = UserData.clean_orphans(data)
                    UserData.set(data)

                # Remove old tasks
                for task in get_children(self.tasks_list.tasks_list):
                    self.tasks_list.tasks_list.remove(task)
                # Remove old trash
                for task in get_children(self.trash_panel.trash_list):
                    self.trash_panel.trash_list.remove(task)
                self._load_tasks()
                Log.info("Tasks imported")
                self.add_toast(_("Tasks Imported"))  # pyright:ignore
                Sync.sync()

            self.import_dialog.open(self, None, finish_import, None)

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
        _create_action("export", _export_tasks, ["<primary>e"])
        _create_action("import", _import_tasks, ["<primary>i"])
        _create_action("shortcuts", _shortcuts, ["<primary>question"])
        _create_action("about", _about)
        _create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
