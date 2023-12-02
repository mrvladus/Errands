# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.widgets.trash_item import TrashItem
from gi.repository import Adw, Gtk, GObject
from errands.widgets.task import Task
from errands.utils.sync import Sync
from errands.utils.logging import Log


class Trash(Adw.Bin):
    def __init__(self, window, tasks_panel):
        super().__init__()
        self.window = window
        self.tasks_panel = tasks_panel
        self.build_ui()

    def build_ui(self):
        # Headerbar
        hb = Adw.HeaderBar(show_title=False, show_back_button=False)
        # Back button
        back_btn = Gtk.Button(icon_name="go-previous-symbolic", visible=False)
        back_btn.bind_property(
            "visible",
            self.tasks_panel.split_view,
            "collapsed",
            GObject.BindingFlags.BIDIRECTIONAL,
        )
        back_btn.connect(
            "clicked", lambda *_: self.tasks_panel.split_view.set_show_sidebar(False)
        )
        hb.pack_start(back_btn)
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
        self.trash_list = Gtk.ListBox(
            margin_top=12,
            margin_bottom=12,
            margin_start=12,
            margin_end=12,
            selection_mode=0,
            css_classes=["boxed-list"],
            vexpand=False,
        )
        self.scrl = Gtk.ScrolledWindow(
            child=Adw.Clamp(child=self.trash_list),
            visible=False,
            propagate_natural_height=True,
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

    def trash_add(self, uid: str) -> None:
        """
        Add item to trash
        """

        self.trash_list.append(TrashItem(uid, self.tasks_panel, self.trash_list))
        self.scrl.set_visible(True)

    def trash_clear(self) -> None:
        """
        Clear unneeded items from trash
        """

        res = UserData.run_sql(
            f"""SELECT uid FROM tasks
            WHERE deleted = 0
            AND list_uid = '{self.tasks_panel.list_uid}'""",
            fetch=True,
        )
        ids = [i[0] for i in res]
        to_remove: list[TrashItem] = []
        for item in get_children(self.trash_list):
            if item.uid in ids:
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
        dialog.add_response("cancel", _("Cancel"))  # type:ignore
        dialog.add_response("delete", _("Delete"))  # type:ignore
        dialog.set_response_appearance("delete", Adw.ResponseAppearance.DESTRUCTIVE)
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
        to_remove: list[Task] = [
            task
            for task in self.tasks_panel.get_all_tasks()
            if task.get_prop("deleted")
        ]
        for task in to_remove:
            task.purge()
        UserData.run_sql(
            f"""DELETE FROM tasks 
            WHERE deleted = 1
            AND list_uid = '{self.tasks_panel.list_uid}'"""
        )
        # Remove trash items widgets
        self.trash_list.remove_all()
        self.scrl.set_visible(False)
        # Sync
        Sync.sync()

    def on_trash_restore(self, _) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Restore Trash")

        # Restore tasks
        tasks: list[Task] = self.tasks_panel.get_all_tasks()
        for task in tasks:
            task.update_prop("deleted", False)
            task.toggle_visibility(True)
            # Update statusbar
            if not task.is_sub_task:
                task.update_status()
            else:
                task.parent.update_status()
            # Expand if needed
            for t in tasks:
                if t.get_prop("parent") == task.uid:
                    task.expand(True)
                    break

        # Clear trash
        self.trash_clear()
        self.tasks_panel.update_status()
