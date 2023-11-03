# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from gi.repository import Adw, Gtk
from errands.widgets.task import Task
from errands.utils.sync import Sync
from errands.utils.logging import Log


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/trash_item.ui")
class TrashItem(Adw.ActionRow):
    __gtype_name__ = "TrashItem"

    def __init__(self, task: dict, tasks_list) -> None:
        super().__init__()
        self.tasks_list = tasks_list
        self.id: str = task["id"]
        self.set_title(task["text"])

    @Gtk.Template.Callback()
    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.id}")

        tasks: list[Task] = self.tasks_list.get_all_tasks()

        def restore_task(id: str = self.id) -> None:
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
        self.tasks_list.update_status()
        self.tasks_list.trash_panel.trash_clear()


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/trash_panel.ui")
class TrashPanel(Adw.Bin):
    __gtype_name__ = "TrashPanel"

    # Template children
    confirm_dialog: Adw.MessageDialog = Gtk.Template.Child()
    clear_trash_btn: Gtk.Button = Gtk.Template.Child()
    trash_list: Gtk.Box = Gtk.Template.Child()
    trash_list_scrl: Gtk.ScrolledWindow = Gtk.Template.Child()

    tasks_list = None

    def __init__(self):
        super().__init__()

    def trash_add(self, task: dict) -> None:
        """
        Add item to trash
        """

        self.trash_list.add(TrashItem(task, self.tasks_list))
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
        # self.confirm_dialog.set_transient_for(self.window)
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
