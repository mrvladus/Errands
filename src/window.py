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
from gi.repository import Gio, Adw, Gtk, GLib
from __main__ import VERSION, PROFILE, APP_ID

from .sub_task import SubTask
from .utils import Animate, GSettings, Log, TaskUtils, UserData
from .task import Task

# from .preferences import PreferencesWindow


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    about_window: Adw.AboutWindow = Gtk.Template.Child()
    clear_trash_btn: Gtk.Button = Gtk.Template.Child()
    delete_completed_tasks_btn: Gtk.Button = Gtk.Template.Child()
    drop_motion_ctrl: Gtk.DropControllerMotion = Gtk.Template.Child()
    export_dialog: Gtk.FileDialog = Gtk.Template.Child()
    import_dialog: Gtk.FileDialog = Gtk.Template.Child()
    main_menu_btn: Gtk.MenuButton = Gtk.Template.Child()
    scroll_up_btn_rev: Gtk.Revealer = Gtk.Template.Child()
    scrolled_window: Gtk.ScrolledWindow = Gtk.Template.Child()
    separator: Gtk.Box = Gtk.Template.Child()
    shortcuts_window: Gtk.ShortcutsWindow = Gtk.Template.Child()
    status: Gtk.Statusbar = Gtk.Template.Child()
    tasks_list: Gtk.Box = Gtk.Template.Child()
    toast_overlay: Adw.ToastOverlay = Gtk.Template.Child()
    toast_copied: Adw.Toast = Gtk.Template.Child()
    toast_err: Adw.Toast = Gtk.Template.Child()
    toast_exported: Adw.Toast = Gtk.Template.Child()
    toast_imported: Adw.Toast = Gtk.Template.Child()
    toggle_trash_btn: Gtk.ToggleButton = Gtk.Template.Child()
    trash_list: Gtk.Box = Gtk.Template.Child()
    trash_list_scrl: Gtk.ScrolledWindow = Gtk.Template.Child()
    trash_scroll_separator: Gtk.Box = Gtk.Template.Child()

    # State
    scrolling: bool = False

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        # Remember window size
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        GSettings.bind("sidebar-open", self.toggle_trash_btn, "active")
        # Add devel style if needed
        if PROFILE == "development":
            self.add_css_class("devel")
        self.get_settings().props.gtk_icon_theme_name = "Adwaita"
        self.create_actions()
        self.load_tasks()

    def add_toast(self, toast: Adw.Toast) -> None:
        self.toast_overlay.add_toast(toast)

    def create_actions(self) -> None:
        """Create actions for main menu"""

        def create_action(name: str, callback: callable, shortcuts=None) -> None:
            action = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            if shortcuts:
                self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
            self.props.application.add_action(action)

        # create_action(
        #     "preferences",
        #     lambda *_: PreferencesWindow(self).show(),
        #     ["<primary>comma"],
        # )
        create_action("export", self.export_tasks, ["<primary>e"])
        create_action("import", self.import_tasks, ["<primary>i"])
        create_action("shortcuts", self.shortcuts, ["<primary>question"])
        create_action("about", self.about)
        create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q", "<primary>w"],
        )

    def load_tasks(self) -> None:
        Log.debug("Loading tasks")
        data: dict = UserData.get()
        for task in data["tasks"]:
            if task["parent"] != "":
                continue
            new_task = Task(task, self)
            self.tasks_list.append(new_task)
            if not task["deleted"]:
                new_task.toggle_visibility()
        self.update_status()
        self.trash_add_items()

    def about(self, *args) -> None:
        """Show about window"""
        self.about_window.props.version = VERSION
        self.about_window.props.application_icon = APP_ID
        self.about_window.show()

    def export_tasks(self, *_) -> None:
        """Show export dialog"""

        def finish_export(_dial, res, _data):
            try:
                file: Gio.File = self.export_dialog.save_finish(res)
            except GLib.GError:
                Log.debug("Export cancelled")
                return
            path = file.get_path()
            with open(path, "w+") as f:
                json.dump(UserData.get(), f, indent=4)
            self.add_toast(self.toast_exported)
            Log.info(f"Export tasks to: {path}")

        self.export_dialog.save(self, None, finish_export, None)

    def import_tasks(self, *_) -> None:
        """Show import dialog"""

        def finish_import(_dial, res, _data):
            Log.info("Importing tasks")

            try:
                file: Gio.File = self.import_dialog.open_finish(res)
            except GLib.GError:
                Log.debug("Import cancelled")
                return

            with open(file.get_path(), "r") as f:
                text = f.read()
                if not UserData.validate(text):
                    self.add_toast(self.toast_err)
                    return
                UserData.set(json.loads(text))

            # Reload tasks
            children = self.tasks_list.observe_children()
            to_remove: list = [
                children.get_item(i) for i in range(children.get_n_items())
            ]
            for task in to_remove:
                self.tasks_list.remove(task)
            self.load_tasks()

            # Reload trash
            children = self.trash_list.observe_children()
            to_remove: list = [
                children.get_item(i) for i in range(children.get_n_items())
            ]
            for task in to_remove:
                self.trash_list.remove(task)
            self.trash_add_items()
            self.add_toast(self.toast_imported)
            Log.info("Tasks imported")

        self.import_dialog.open(self, None, finish_import, None)

    def shortcuts(self, *_):
        self.shortcuts_window.set_transient_for(self)
        self.shortcuts_window.show()

    def trash_add(self, task: dict) -> None:
        self.trash_list.append(TrashItem(task, self))
        self.trash_list_scrl.set_visible(True)

    def trash_add_items(self) -> None:
        """Populate trash on startup"""

        tasks: list[dict] = UserData.get()["tasks"]
        deleted_count: int = 0
        for task in tasks:
            if task["deleted"]:
                deleted_count += 1
                self.trash_add(task)
        self.trash_list_scrl.set_visible(deleted_count > 0)

    def trash_clear(self) -> None:
        """Clear unneeded items from trash"""

        tasks: list[dict] = UserData.get()["tasks"]
        children = self.trash_list.observe_children()
        items = [children.get_item(i) for i in range(children.get_n_items())]
        deleted_count: int = 0
        for task in tasks:
            if not task["deleted"]:
                for item in items:
                    if item.id == task["id"]:
                        self.trash_list.remove(item)
            else:
                deleted_count += 1
        self.trash_list_scrl.set_visible(deleted_count > 0)

    def trash_update(self, tasks: list[dict] = UserData.get()["tasks"]) -> None:
        deleted_count: int = 0
        for task in tasks:
            if task["deleted"]:
                deleted_count += 1
        self.trash_list_scrl.set_visible(deleted_count > 0)

    def update_status(self) -> None:
        """Update progress bar on the top"""
        n_total = 0
        n_completed = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == "":
                if not task["deleted"]:
                    n_total += 1
                    if task["completed"]:
                        n_completed += 1
        # Update progress bar animation
        Animate.property(
            self.status,
            "fraction",
            self.status.props.fraction,
            n_completed / n_total if n_total > 0 else 0,
            250,
        )
        # Show delete completed button
        self.delete_completed_tasks_btn.set_sensitive(n_completed > 0)

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_dnd_scroll(self, _motion, _x, y) -> bool:
        """Autoscroll while dragging task"""

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
        """Show scroll up button"""

        self.scroll_up_btn_rev.set_reveal_child(adj.get_value() > 0)
        if adj.get_value() > 0:
            self.separator.add_css_class("separator")
        else:
            self.separator.remove_css_class("separator")

    @Gtk.Template.Callback()
    def on_scroll_up_btn_clicked(self, _) -> None:
        """Scroll up"""

        Animate.scroll(self.scrolled_window, False)

    @Gtk.Template.Callback()
    def on_task_added(self, entry: Gtk.Entry) -> None:
        """Add new task"""

        text: str = entry.props.text
        # Check for empty string or task exists
        if text == "":
            return

        # Add new task
        new_data: dict = UserData.get()
        new_task: dict = TaskUtils.new_task(text)
        new_data["tasks"].append(new_task)
        UserData.set(new_data)
        task = Task(new_task, self)
        self.tasks_list.append(task)
        task.toggle_visibility()
        # Clear entry
        entry.props.text = ""
        # Scroll to the end
        Animate.scroll(self.scrolled_window, True)
        self.update_status()

    @Gtk.Template.Callback()
    def on_toggle_trash_btn(self, btn: Gtk.ToggleButton) -> None:
        if btn.props.active:
            self.clear_trash_btn.grab_focus()
        else:
            btn.grab_focus()

    @Gtk.Template.Callback()
    def on_trash_scroll(self, adj) -> None:
        """Show scroll up button"""

        if adj.get_value() > 0:
            self.trash_scroll_separator.add_css_class("separator")
        else:
            self.trash_scroll_separator.remove_css_class("separator")

    @Gtk.Template.Callback()
    def on_delete_completed_tasks_btn_clicked(self, _) -> None:
        """Hide completed tasks"""

        tasks = self.tasks_list.observe_children()
        for i in range(tasks.get_n_items()):
            task = tasks.get_item(i)
            if task.task["completed"] and not task.task["deleted"]:
                task.delete(update_sts=False)
        self.update_status()

    @Gtk.Template.Callback()
    def on_trash_clear(self, _) -> None:
        Log.info("Clear Trash")
        children = self.tasks_list.observe_children()
        to_remove = [
            children.get_item(i)
            for i in range(children.get_n_items())
            if children.get_item(i).task["deleted"]
        ]
        # Remove tasks
        for task in to_remove:
            self.tasks_list.remove(task)

        # Remove data
        data: dict = UserData.get()
        data["tasks"] = [t for t in data["tasks"] if not t["deleted"]]
        UserData.set(data)

        # Remove trash items
        children = self.trash_list.observe_children()
        items = [children.get_item(i) for i in range(children.get_n_items())]
        for item in items:
            self.trash_list.remove(item)

        self.trash_list_scrl.set_visible(False)

    @Gtk.Template.Callback()
    def on_trash_restore(self, _) -> None:
        Log.info("Restore Trash")

        data: dict = UserData.get()
        for task in data["tasks"]:
            task["deleted"] = False
        UserData.set(data)

        # Restore tasks
        def restore_tasks(list: Gtk.Box) -> None:
            """Recursive func for restoring tasks"""

            tasks = list.observe_children()
            for i in range(tasks.get_n_items()):
                task = tasks.get_item(i)
                if task.task["deleted"]:
                    task.task["deleted"] = False
                    task.toggle_visibility(True)
                # If has sub-tasks: call restore_tasks
                if hasattr(task, "sub_tasks"):
                    restore_tasks(task.sub_tasks)
                # Update statusbar if task is toplevel
                if task.task["parent"] == "":
                    task.update_statusbar()

        restore_tasks(self.tasks_list)

        # Clear trash
        self.trash_clear()
        self.update_status()

    @Gtk.Template.Callback()
    def on_trash_drop(self, _drop, task: Task | SubTask, _x, _y) -> None:
        task.toggle_visibility()
        task.task["deleted"] = True
        task.update_data()
        self.trash_add(task.task)
        self.update_status()


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/trash_item.ui")
class TrashItem(Gtk.Box):
    __gtype_name__ = "TrashItem"

    label = Gtk.Template.Child()

    def __init__(self, task: dict, window: Window) -> None:
        super().__init__()
        self.window = window
        self.id = task["id"]
        self.label.props.label = task["text"]

    @Gtk.Template.Callback()
    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info("Restore: " + self.label.props.label)

        # Update data
        data: dict = UserData.get()
        ids: list[str] = []
        for task in data["tasks"]:
            if task["id"] == self.id:

                def restore_parent(id: str):
                    for parent in data["tasks"]:
                        if parent["id"] == id:
                            parent["deleted"] = False
                            ids.append(parent["id"])
                            if parent["parent"] != "":
                                restore_parent(parent["parent"])

                task["deleted"] = False
                ids.append(task["id"])
                if task["parent"] != "":
                    restore_parent(task["parent"])
                UserData.set(data)
                break

        # Update UI
        def restore_tasks(list: Gtk.Box) -> None:
            """Recursive func for restoring tasks"""

            tasks = list.observe_children()
            for i in range(tasks.get_n_items()):
                task = tasks.get_item(i)
                if task.task["id"] in ids:
                    task.task["deleted"] = False
                    task.toggle_visibility(True)
                # If has sub-tasks: call restore_tasks
                if hasattr(task, "sub_tasks"):
                    restore_tasks(task.sub_tasks)
                # Update statusbar if task is toplevel
                if task.task["parent"] == "":
                    task.update_statusbar()

        restore_tasks(self.window.tasks_list)

        self.window.update_status()
        self.window.trash_clear()
