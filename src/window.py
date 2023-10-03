# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import json
import os
from gi.repository import Gio, Adw, Gtk, GLib
from __main__ import VERSION, PROFILE, APP_ID


from .preferences import PreferencesWindow
from .sync import Sync
from .task import Task
from .utils import Animate, GSettings, Log, TaskUtils, UserData, get_children, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    # - Template children - #
    about_window: Adw.AboutWindow = Gtk.Template.Child()
    clear_trash_btn: Gtk.Button = Gtk.Template.Child()
    confirm_dialog: Adw.MessageDialog = Gtk.Template.Child()
    delete_completed_tasks_btn: Gtk.Button = Gtk.Template.Child()
    drop_motion_ctrl: Gtk.DropControllerMotion = Gtk.Template.Child()
    export_dialog: Gtk.FileDialog = Gtk.Template.Child()
    import_dialog: Gtk.FileDialog = Gtk.Template.Child()
    main_menu_btn: Gtk.MenuButton = Gtk.Template.Child()
    scroll_up_btn_rev: Gtk.Revealer = Gtk.Template.Child()
    scrolled_window: Gtk.ScrolledWindow = Gtk.Template.Child()
    shortcuts_window: Gtk.ShortcutsWindow = Gtk.Template.Child()
    split_view: Adw.OverlaySplitView = Gtk.Template.Child()
    tasks_list: Gtk.Box = Gtk.Template.Child()
    title: Adw.WindowTitle = Gtk.Template.Child()
    toast_overlay: Adw.ToastOverlay = Gtk.Template.Child()
    toast_copied: Adw.Toast = Gtk.Template.Child()
    toast_err: Adw.Toast = Gtk.Template.Child()
    toast_exported: Adw.Toast = Gtk.Template.Child()
    toast_imported: Adw.Toast = Gtk.Template.Child()
    toggle_trash_btn: Gtk.ToggleButton = Gtk.Template.Child()
    trash_list: Gtk.Box = Gtk.Template.Child()
    trash_list_scrl: Gtk.ScrolledWindow = Gtk.Template.Child()

    # - State - #
    scrolling: bool = False  # Is window scrolling
    startup: bool = True

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        # Remember window state
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        GSettings.bind("sidebar-open", self.toggle_trash_btn, "active")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self.create_actions()
        Sync.init(self)
        Sync.sync_blocking(True)
        self.load_tasks()
        self.startup = False

    def add_task(self, task: dict) -> None:
        new_task = Task(task, self)
        self.tasks_list.append(new_task)
        if not task["deleted"]:
            new_task.toggle_visibility(True)

    def add_toast(self, toast: Adw.Toast) -> None:
        self.toast_overlay.add_toast(toast)

    def create_actions(self) -> None:
        """
        Create actions for main menu
        """

        def create_action(name: str, callback: callable, shortcuts=None) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            if shortcuts:
                self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
            self.props.application.add_action(action)

        def about(*_) -> None:
            """
            Show about window
            """

            self.about_window.props.version = VERSION
            self.about_window.props.application_icon = APP_ID
            self.about_window.show()

        def export_tasks(*_) -> None:
            """
            Show export dialog
            """

            def finish_export(_dial, res, _data):
                try:
                    file: Gio.File = self.export_dialog.save_finish(res)
                except GLib.GError:
                    Log.debug("Export cancelled")
                    return
                path: str = file.get_path()
                with open(path, "w+") as f:
                    json.dump(UserData.get(), f, indent=4)
                self.add_toast(self.toast_exported)
                Log.info(f"Export tasks to: {path}")

            self.export_dialog.save(self, None, finish_export, None)

        def import_tasks(*_) -> None:
            """
            Show import dialog
            """

            def finish_import(_dial, res, _data):
                Log.info("Importing tasks")

                try:
                    file: Gio.File = self.import_dialog.open_finish(res)
                except GLib.GError:
                    Log.debug("Import cancelled")
                    return

                with open(file.get_path(), "r") as f:
                    text: str = f.read()
                    if not UserData.validate(text):
                        self.add_toast(self.toast_err)
                        return
                    UserData.set(json.loads(text))

                # Remove old tasks
                for task in self.get_toplevel_tasks():
                    self.tasks_list.remove(task)
                # Remove old trash
                for task in get_children(self.trash_list):
                    self.trash_list.remove(task)
                self.load_tasks()
                self.add_toast(self.toast_imported)
                Log.info("Tasks imported")

            self.import_dialog.open(self, None, finish_import, None)

        def open_log(*_) -> None:
            """
            Open log file with default text editor
            """
            path = os.path.join(GLib.get_user_data_dir(), "list", "log.txt")
            GLib.spawn_command_line_async(f"xdg-open {path}")

        def shortcuts(*_) -> None:
            """
            Show shortcuts window
            """

            self.shortcuts_window.set_transient_for(self)
            self.shortcuts_window.show()

        create_action(
            "preferences",
            lambda *_: PreferencesWindow(self).show(),
            ["<primary>comma"],
        )
        create_action("export", export_tasks, ["<primary>e"])
        create_action("import", import_tasks, ["<primary>i"])
        create_action("shortcuts", shortcuts, ["<primary>question"])
        create_action("about", about)
        create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )
        create_action("open_log", open_log)

    def get_all_tasks(self) -> list[Task]:
        """
        Get list of all tasks widgets including sub-tasks
        """

        tasks: list[Task] = []

        def append_tasks(items: list[Task]):
            for task in items:
                tasks.append(task)
                children: list[Task] = get_children(task.tasks_list)
                if len(children) > 0:
                    append_tasks(children)

        append_tasks(get_children(self.tasks_list))
        return tasks

    def get_toplevel_tasks(self) -> list[Task]:
        return get_children(self.tasks_list)

    def load_tasks(self) -> None:
        Log.debug("Loading tasks")
        for task in UserData.get()["tasks"]:
            if not task["parent"]:
                self.add_task(task)
        self.update_status()
        # Expand tasks if needed
        if GSettings.get("expand-on-startup"):
            for task in self.get_all_tasks():
                if len(get_children(task.tasks_list)) > 0:
                    task.expand(True)

    def update_ui(self) -> None:
        Log.debug("Updating UI")

        # Update existing tasks
        tasks: list[Task] = self.get_all_tasks()
        data_tasks: list[dict] = UserData.get()["tasks"]
        to_change_parent: list[dict] = []
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
                self.add_task(task)
            else:
                for t in tasks:
                    if t.task["id"] == task["parent"]:
                        t.add_task(task)
                        break

        # Create new tasks
        tasks_ids: list[str] = [task.task["id"] for task in self.get_all_tasks()]
        for task in data_tasks:
            if task["id"] not in tasks_ids:
                # Add toplevel task and its sub-tasks
                if task["parent"] == "":
                    self.add_task(task)
                # Add sub-task and its sub-tasks
                else:
                    for t in self.get_all_tasks():
                        if t.task["id"] == task["parent"]:
                            t.add_task(task)
                tasks_ids = [task.task["id"] for task in self.get_all_tasks()]

    def trash_add(self, task: dict) -> None:
        """
        Add item to trash
        """

        self.trash_list.append(TrashItem(task, self))
        self.trash_list_scrl.set_visible(True)

    def trash_clear(self) -> None:
        """
        Clear unneeded items from trash
        """

        tasks: list[dict] = UserData.get()["tasks"]
        to_remove: list = []
        trash_children: list[TrashItem] = get_children(self.trash_list)
        for task in tasks:
            if not task["deleted"]:
                for item in trash_children:
                    if item.id == task["id"]:
                        to_remove.append(item)
        for item in to_remove:
            self.trash_list.remove(item)

        self.trash_list_scrl.set_visible(len(get_children(self.trash_list)) > 0)

    def update_status(self) -> None:
        """
        Update status bar on the top
        """

        # Set status
        n_total = 0
        n_completed = 0
        tasks: list[Task] = self.get_all_tasks()
        for task in tasks:
            if task.task["parent"] == "":
                if not task.task["deleted"]:
                    n_total += 1
                    if task.task["completed"]:
                        n_completed += 1
        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}"  # pyright: ignore
            if n_total > 0
            else ""
        )

        # Set state for delete completed button
        n_completed = 0
        n_deleted = 0
        for task in tasks:
            if not task.task["deleted"]:
                if task.task["completed"]:
                    n_completed += 1
            else:
                n_deleted += 1
        self.delete_completed_tasks_btn.set_sensitive(n_completed > 0)
        # Show trash
        self.trash_list_scrl.set_visible(n_deleted > 0)

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_dnd_scroll(self, _motion, _x, y) -> bool:
        """
        Autoscroll while dragging task
        """

        def auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.drop_motion_ctrl.contains_pointer():
                return False
            adj = self.scrolled_window.get_vadjustment()
            if scroll_up:
                adj.set_value(adj.get_value() - 2)
                return True
            else:
                adj.set_value(adj.get_value() + 2)
                return True

        MARGIN = 50
        height = self.scrolled_window.get_allocation().height
        if y < MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, auto_scroll, True)
        elif y > height - MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, auto_scroll, False)
        else:
            self.scrolling = False

    @Gtk.Template.Callback()
    def on_scroll(self, adj) -> None:
        """
        Show scroll up button
        """

        self.scroll_up_btn_rev.set_reveal_child(adj.get_value() > 0)

    @Gtk.Template.Callback()
    def on_scroll_up_btn_clicked(self, _) -> None:
        """
        Scroll up
        """

        Animate.scroll(self.scrolled_window, False)

    @Gtk.Template.Callback()
    def on_task_added(self, entry: Gtk.Entry) -> None:
        """
        Add new task
        """

        text: str = entry.props.text
        # Check for empty string or task exists
        if text == "":
            return
        # Add new task
        new_data: dict = UserData.get()
        new_task: dict = TaskUtils.new_task(text)
        new_data["tasks"].append(new_task)
        UserData.set(new_data)
        self.add_task(new_task)
        # Clear entry
        entry.props.text = ""
        # Scroll to the end
        Animate.scroll(self.scrolled_window, True)
        self.update_status()
        # Sync
        Sync.sync()

    @Gtk.Template.Callback()
    def on_toggle_trash_btn(self, btn: Gtk.ToggleButton) -> None:
        """
        Move focus to sidebar
        """
        if btn.props.active:
            self.clear_trash_btn.grab_focus()
        else:
            btn.grab_focus()

    @Gtk.Template.Callback()
    def on_delete_completed_tasks_btn_clicked(self, _) -> None:
        """
        Hide completed tasks
        """
        for task in self.get_all_tasks():
            if task.task["completed"] and not task.task["deleted"]:
                task.delete()
        self.update_status()

    @Gtk.Template.Callback()
    def on_sync_btn_clicked(self, btn) -> None:
        Sync.sync(True)

    @Gtk.Template.Callback()
    def on_trash_clear(self, _) -> None:
        self.confirm_dialog.show()

    @Gtk.Template.Callback()
    def on_trash_clear_confirm(self, _, res) -> None:
        """
        Remove all trash items and tasks
        """

        if res == "cancel":
            Log.debug("Clear Trash cancelled")
            return

        Log.info("Clear Trash")

        # Remove widgets and data
        data: dict = UserData.get()
        data["deleted"] = [task["id"] for task in data["tasks"] if task["deleted"]]
        data["tasks"] = [task for task in data["tasks"] if not task["deleted"]]
        UserData.set(data)
        to_remove = [task for task in self.get_all_tasks() if task.task["deleted"]]
        for task in to_remove:
            task.purge()
        # Remove trash items widgets
        for item in get_children(self.trash_list):
            self.trash_list.remove(item)
        self.trash_list_scrl.set_visible(False)
        # Sync
        Sync.sync()

    @Gtk.Template.Callback()
    def on_trash_close(self, _) -> None:
        self.split_view.set_show_sidebar(False)

    @Gtk.Template.Callback()
    def on_trash_restore(self, _) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Restore Trash")

        data: dict = UserData.get()
        for task in data["tasks"]:
            task["deleted"] = False
        UserData.set(data)

        # Restore tasks
        tasks: list[Task] = self.get_all_tasks()
        for task in tasks:
            if task.task["deleted"]:
                task.task["deleted"] = False
                task.update_data()
                task.toggle_visibility(True)
                # Update statusbar
                if not task.task["parent"]:
                    task.update_status()
                else:
                    task.parent.update_status()
                # Expand if needed
                for t in tasks:
                    if t.task["parent"] == task.task["id"]:
                        task.expand(True)
                        break

        # Clear trash
        self.trash_clear()
        self.update_status()

    @Gtk.Template.Callback()
    def on_trash_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        Move task to trash via dnd
        """

        task.delete()
        self.update_status()


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/trash_item.ui")
class TrashItem(Gtk.Box):
    __gtype_name__ = "TrashItem"

    label = Gtk.Template.Child()

    def __init__(self, task: dict, window: Window) -> None:
        super().__init__()
        self.window: Window = window
        self.id: str = task["id"]
        self.label.props.label = task["text"]

    def __repr__(self) -> str:
        return f"TrashItem({self.id})"

    @Gtk.Template.Callback()
    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.id}")

        tasks: list[Task] = self.window.get_all_tasks()

        def restore_task(id: str = self.id):
            for task in tasks:
                if task.task["id"] == id:
                    task.task["deleted"] = False
                    task.update_data()
                    task.toggle_visibility(True)
                    if task.task["parent"]:
                        task.parent.expand(True)
                        restore_task(task.task["parent"])
                    break

        restore_task()
        self.window.update_status()
        self.window.trash_clear()
