# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from errands.widgets.trash_item import TrashItem
from gi.repository import Adw, Gtk, Gio, GLib, Gdk
from errands.widgets.task import Task
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from errands.utils.logging import Log
from errands.utils.tasks import task_to_ics


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/trash_panel.ui")
class TrashPanel(Adw.Bin):
    __gtype_name__ = "TrashPanel"

    confirm_dialog: Adw.MessageDialog = Gtk.Template.Child()
    clear_trash_btn: Gtk.Button = Gtk.Template.Child()
    trash_list: Gtk.Box = Gtk.Template.Child()
    trash_list_scrl: Gtk.ScrolledWindow = Gtk.Template.Child()

    def __init__(self, window):
        super().__init__()
        self.window = window

    def trash_add(self, task: dict) -> None:
        """
        Add item to trash
        """

        self.trash_list.append(TrashItem(task, self.window))
        self.trash_list_scrl.set_visible(True)

    def trash_clear(self) -> None:
        """
        Clear unneeded items from trash
        """

        tasks: list[UserDataTask] = UserData.get()["tasks"]
        to_remove: list[TrashItem] = []
        trash_children: list[TrashItem] = get_children(self.trash_list)
        for task in tasks:
            if not task["deleted"]:
                for item in trash_children:
                    if item.id == task["id"]:
                        to_remove.append(item)
        for item in to_remove:
            self.trash_list.remove(item)

        self.trash_list_scrl.set_visible(len(get_children(self.trash_list)) > 0)

    @Gtk.Template.Callback()
    def on_trash_clear(self, _) -> None:
        Log.debug("Show confirm dialog")
        self.confirm_dialog.set_transient_for(self.window)
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
        data: UserDataDict = UserData.get()
        data["deleted"] = [task["id"] for task in data["tasks"] if task["deleted"]]
        data["tasks"] = [task for task in data["tasks"] if not task["deleted"]]
        UserData.set(data)
        to_remove: list[Task] = [
            task for task in self.window.get_all_tasks() if task.task["deleted"]
        ]
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
        self.window.split_view.set_show_sidebar(False)

    @Gtk.Template.Callback()
    def on_trash_restore(self, _) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Restore Trash")

        # Restore tasks
        tasks: list[Task] = self.window.get_all_tasks()
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
        self.window.update_status()

    @Gtk.Template.Callback()
    def on_trash_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        Move task to trash via dnd
        """
        Log.debug(f"Drop task to trash: {task.task['id']}")

        task.delete()
        self.window.update_status()
