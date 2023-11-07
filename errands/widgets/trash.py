# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from errands.widgets.trash_item import TrashItem
from gi.repository import Adw, Gtk, GObject
from errands.widgets.task import Task
from errands.utils.sync import Sync
from errands.utils.logging import Log


class Trash(Adw.Bin):
    def __init__(self, window):
        super().__init__()
        self.window = window
        self.build_ui()

    def build_ui(self):
        # Headerbar
        hb = Adw.HeaderBar(show_title=False)
        # Clear button
        clear_btn = Gtk.Button(
            icon_name="user-trash-full-symbolic",
            valign="center",
            tooltip_text=_("Clear"),  # type:ignore
        )
        clear_btn.connect("clicked", self.on_trash_clear)
        hb.pack_start(clear_btn)
        # Restore button
        restore_btn = Gtk.Button(
            icon_name="edit-redo-symbolic",
            valign="center",
            tooltip_text=_("Restore All"),  # type:ignore
        )
        restore_btn.connect("clicked", self.on_trash_restore)
        hb.pack_end(restore_btn)
        # Status
        status = Adw.StatusPage(
            icon_name="user-trash-symbolic",
            title=_("Empty Trash"),  # type:ignore
            description=_("No deleted items"),  # type:ignore
            vexpand=True,
        )
        status.add_css_class("compact")
        # Trash list
        self.trash_list = Adw.PreferencesGroup(
            margin_top=12,
            margin_bottom=12,
            margin_start=12,
            margin_end=12,
        )
        self.scrl = Gtk.ScrolledWindow(
            vexpand=True,
            child=Adw.Clamp(child=self.trash_list),
        )
        self.scrl.bind_property(
            "visible",
            status,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        self.scrl.bind_property(
            "visible", clear_btn, "visible", GObject.BindingFlags.SYNC_CREATE
        )
        self.scrl.bind_property(
            "visible", restore_btn, "visible", GObject.BindingFlags.SYNC_CREATE
        )
        # Box
        box = Gtk.Box(orientation="vertical")
        box.append(status)
        box.append(self.scrl)
        # Toolbar View
        toolbar_view = Adw.ToolbarView(content=box)
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def trash_add(self, task: dict) -> None:
        """
        Add item to trash
        """

        self.trash_list.add(TrashItem(task, self.tasks_list))
        self.scrl.set_visible(True)

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

        self.scrl.set_visible(len(get_children(self.trash_list)) > 0)

    def on_trash_clear(self, btn) -> None:
        Log.debug("Show confirm dialog")
        dialog = Adw.MessageDialog(
            transient_for=self.window,
            hide_on_close=True,
            heading=_("Are you sure?"),  # type:ignore
            body=_("Tasks will be permanently deleted"),  # type:ignore
            default_response="delete",
            close_response="cancel",
        )
        dialog.add_response("delete", _("Delete"))  # type:ignore
        dialog.set_response_appearance("delete", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.add_response("cancel", _("Cancel"))  # type:ignore
        dialog.connect("response", self.on_trash_clear_confirm)
        dialog.show()

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
        self.scrl.set_visible(False)
        # Sync
        Sync.sync()

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
