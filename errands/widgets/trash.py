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
    def __init__(self, window):
        super().__init__()
        self.window = window
        self.stack: Adw.ViewStack = window.stack
        self.build_ui()
        self.update_status()

    def build_ui(self):
        # Headerbar
        hb = Adw.HeaderBar()
        hb.set_title_widget(Adw.WindowTitle(title=_("Trash")))  # type:ignore
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
        self.status = Adw.StatusPage(
            icon_name="user-trash-symbolic",
            title=_("Empty Trash"),  # type:ignore
            description=_("No deleted items"),  # type:ignore
            vexpand=True,
        )
        self.status.add_css_class("compact")
        # Trash list
        self.trash_list = Gtk.Box(
            orientation="vertical",
            margin_top=12,
            margin_bottom=12,
            margin_start=12,
            margin_end=12,
            spacing=12,
            css_classes=["boxed-list"],
            vexpand=False,
        )
        scrl = Gtk.ScrolledWindow(
            child=Adw.Clamp(
                child=self.trash_list,
                maximum_size=850,
                tightening_threshold=300,
            ),
            visible=False,
            propagate_natural_height=True,
        )
        scrl.bind_property(
            "visible",
            self.status,
            "visible",
            GObject.BindingFlags.INVERT_BOOLEAN | GObject.BindingFlags.BIDIRECTIONAL,
        )
        scrl.bind_property(
            "visible", clear_btn, "visible", GObject.BindingFlags.SYNC_CREATE
        )
        scrl.bind_property(
            "visible", restore_btn, "visible", GObject.BindingFlags.SYNC_CREATE
        )
        # Box
        box = Gtk.Box(orientation="vertical")
        box.append(self.status)
        box.append(scrl)
        # Toolbar View
        toolbar_view = Adw.ToolbarView(content=box)
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def trash_add(self, task_widget) -> None:
        """
        Add item to trash
        """

        self.trash_list.append(TrashItem(task_widget, self))
        self.status.set_visible(False)

    def update_status(self):
        deleted_uids = [
            i[0]
            for i in UserData.run_sql(
                "SELECT uid FROM tasks WHERE trash = 1 AND deleted = 0", fetch=True
            )
        ]
        self.status.set_visible(len(deleted_uids) == 0)

    def on_trash_clear(self, btn) -> None:
        def _confirm(_, res):
            if res == "cancel":
                Log.debug("Clear Trash cancelled")
                return

            Log.info("Trash: Clear")

            UserData.run_sql(
                f"""UPDATE tasks
                SET deleted = 1
                WHERE trash = 1""",
            )

            # Remove tasks
            task_lists: list = []
            pages = self.stack.get_pages()
            for i in range(pages.get_n_items()):
                child = pages.get_item(i).get_child()
                if hasattr(child, "get_all_tasks"):
                    task_lists.append(child)
                    tasks = child.get_all_tasks()
                    for task in tasks:
                        if task.get_prop("trash"):
                            task.purge()

            for row in get_children(self.trash_list):
                self.trash_list.remove(row)

            self.status.set_visible(True)
            self.update_status()
            # Sync
            Sync.sync()

        Log.debug("Trash: Show confirm dialog")

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
        dialog.connect("response", _confirm)
        dialog.show()

    def on_trash_restore(self, _) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Trash: Restore")

        # Get all tasks
        tasks: list[Task] = []
        task_lists: list = []
        pages = self.stack.get_pages()
        for i in range(pages.get_n_items()):
            child = pages.get_item(i).get_child()
            if hasattr(child, "get_all_tasks"):
                task_lists.append(child)
                tasks.extend(child.get_all_tasks())

        for task in tasks:
            task.update_props(["trash"], [False])
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

        for list in task_lists:
            list.update_status()

        for row in get_children(self.trash_list):
            self.trash_list.remove(row)

        self.update_status()
