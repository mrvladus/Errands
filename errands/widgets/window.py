# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import json
from errands.widgets.tasks_list import TasksList
from errands.widgets.trash_panel import TrashPanel
from gi.repository import Gio, Adw, Gtk, GLib, GObject
from __main__ import VERSION, APP_ID

# Import modules
from errands.widgets.preferences import PreferencesWindow
from errands.widgets.task_details import TaskDetails
from errands.widgets.task import Task
from errands.utils.sync import Sync
from errands.utils.animation import scroll
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from errands.utils.markup import Markup


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    # Composite template children
    GObject.type_ensure(TrashPanel)
    GObject.type_ensure(TaskDetails)
    GObject.type_ensure(TasksList)

    # - Template children - #
    about_window: Adw.AboutWindow = Gtk.Template.Child()
    delete_completed_tasks_btn_rev: Gtk.Revealer = Gtk.Template.Child()
    export_dialog: Gtk.FileDialog = Gtk.Template.Child()
    import_dialog: Gtk.FileDialog = Gtk.Template.Child()
    main_menu_btn: Gtk.MenuButton = Gtk.Template.Child()
    scroll_up_btn_rev: Gtk.Revealer = Gtk.Template.Child()
    shortcuts_window: Gtk.ShortcutsWindow = Gtk.Template.Child()
    split_view: Adw.OverlaySplitView = Gtk.Template.Child()
    sync_btn: Gtk.Button = Gtk.Template.Child()
    title: Adw.WindowTitle = Gtk.Template.Child()
    toast_overlay: Adw.ToastOverlay = Gtk.Template.Child()
    toggle_trash_btn: Gtk.ToggleButton = Gtk.Template.Child()
    sidebar = Gtk.Template.Child()

    trash_panel = Gtk.Template.Child()
    task_details = Gtk.Template.Child()
    tasks_list = Gtk.Template.Child()

    # - State - #
    scrolling: bool = False  # Is window scrolling
    startup: bool = True

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        # Remember window state
        Log.debug("Getting window settings")
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        GSettings.bind("sidebar-open", self.toggle_trash_btn, "active")
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

            self.shortcuts_window.set_transient_for(self)
            self.shortcuts_window.show()

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

    def update_ui(self) -> None:
        Log.debug("Updating UI")

        # Update existing tasks
        tasks: list[Task] = self.tasks_list.get_all_tasks()
        data_tasks: list[UserDataTask] = UserData.get()["tasks"]
        to_change_parent: list[UserDataTask] = []
        to_remove: list[Task] = []
        for task in tasks:
            for t in data_tasks:
                if task.task["id"] == t["id"]:
                    # If parent is changed
                    if task.task["parent"] != t["parent"]:
                        to_change_parent.append(t)
                        to_remove.append(task)
                        break
                    # If text changed
                    if task.task["text"] != t["text"]:
                        task.task["text"] = t["text"]
                        task.text = Markup.find_url(Markup.escape(task.task["text"]))
                        task.task_row.props.title = task.text
                    # If completion changed
                    if task.task["completed"] != t["completed"]:
                        task.completed_btn.props.active = t["completed"]

        # Remove old tasks
        for task in to_remove:
            task.purge()

        # Change parents
        for task in to_change_parent:
            if task["parent"] == "":
                self.tasks_list.add_task(task)
            else:
                for t in tasks:
                    if t.task["id"] == task["parent"]:
                        t.add_task(task)
                        break

        # Create new tasks
        tasks_ids: list[str] = [
            task.task["id"] for task in self.tasks_list.get_all_tasks()
        ]
        for task in data_tasks:
            if task["id"] not in tasks_ids:
                # Add toplevel task and its sub-tasks
                if task["parent"] == "":
                    self.tasks_list.add_task(task)
                # Add sub-task and its sub-tasks
                else:
                    for t in self.tasks_list.get_all_tasks():
                        if t.task["id"] == task["parent"]:
                            t.add_task(task)
                tasks_ids = [
                    task.task["id"] for task in self.tasks_list.get_all_tasks()
                ]

        # Remove tasks
        ids = [t["id"] for t in UserData.get()["tasks"]]
        for task in self.tasks_list.get_all_tasks():
            if task.task["id"] not in ids:
                task.purge()

    def update_status(self) -> None:
        """
        Update status bar on the top
        """

        tasks: list[UserDataTask] = UserData.get()["tasks"]
        n_total: int = 0
        n_completed: int = 0
        n_all_deleted: int = 0
        n_all_completed: int = 0

        for task in tasks:
            if task["parent"] == "":
                if not task["deleted"]:
                    n_total += 1
                    if task["completed"]:
                        n_completed += 1
            if not task["deleted"]:
                if task["completed"]:
                    n_all_completed += 1
            else:
                n_all_deleted += 1

        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}"  # pyright: ignore
            if n_total > 0
            else ""
        )
        self.delete_completed_tasks_btn_rev.set_reveal_child(n_all_completed > 0)
        self.trash_panel.trash_list_scrl.set_visible(n_all_deleted > 0)

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_toggle_trash_btn(self, btn: Gtk.ToggleButton) -> None:
        """
        Move focus to sidebar
        """
        if btn.get_active():
            try:
                self.trash_panel.clear_trash_btn.grab_focus()
            except:
                pass
        else:
            btn.grab_focus()

    @Gtk.Template.Callback()
    def on_delete_completed_tasks_btn_clicked(self, _) -> None:
        """
        Hide completed tasks and move them to trash
        """
        Log.info("Delete completed tasks")

        for task in self.tasks_list.get_all_tasks():
            if task.task["completed"] and not task.task["deleted"]:
                task.delete()
        self.update_status()

    @Gtk.Template.Callback()
    def on_scroll_up_btn_clicked(self, _) -> None:
        """
        Scroll up
        """

        scroll(self.tasks_list.scrolled_window, False)

    @Gtk.Template.Callback()
    def on_sync_btn_clicked(self, btn) -> None:
        Sync.sync(True)

    @Gtk.Template.Callback()
    def on_width_changed(self, *_) -> None:
        """
        Breakpoints simulator
        """
        width: int = self.props.default_width
        self.scroll_up_btn_rev.set_visible(width > 400)
        self.split_view.set_collapsed(width < 1030)
        self.sidebar.set_collapsed(width < 730)
