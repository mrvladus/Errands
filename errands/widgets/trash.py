# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.widgets.components import Box, Button
from gi.repository import Adw, Gtk, GObject
from errands.widgets.task import Task
from errands.lib.sync.sync import Sync
from errands.lib.logging import Log


class Trash(Adw.Bin):
    def __init__(self, window):
        super().__init__()
        self.window = window
        self.stack: Adw.ViewStack = window.stack
        self._build_ui()
        self.update_status()

    def _build_ui(self) -> None:
        # Headerbar
        hb = Adw.HeaderBar(title_widget=Adw.WindowTitle(title=_("Trash")))
        # Clear button
        clear_btn = Button(
            icon_name="errands-trash-symbolic",
            on_click=self.on_trash_clear,
            valign="center",
            tooltip_text=_("Clear"),
        )
        hb.pack_start(clear_btn)
        # Restore button
        restore_btn = Button(
            icon_name="edit-redo-symbolic",
            on_click=self.on_trash_restore,
            valign="center",
            tooltip_text=_("Restore All"),
        )
        hb.pack_end(restore_btn)
        # Status
        self.status = Adw.StatusPage(
            icon_name="errands-trash-symbolic",
            title=_("Empty Trash"),
            description=_("No deleted items"),
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
        # Toolbar View
        toolbar_view = Adw.ToolbarView(
            content=Box(children=[self.status, scrl], orientation="vertical")
        )
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def trash_add(self, task_widget) -> None:
        self.trash_list.append(TrashItem(task_widget, self))
        self.status.set_visible(False)

    def update_status(self) -> None:
        deleted_uids = [
            i[0]
            for i in UserData.run_sql(
                "SELECT uid FROM tasks WHERE trash = 1 AND deleted = 0", fetch=True
            )
        ]
        self.status.set_visible(len(deleted_uids) == 0)

    def on_trash_clear(self, btn) -> None:
        def _confirm(_, res) -> None:
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
            heading=_("Are you sure?"),
            body=_("Tasks will be permanently deleted"),
            default_response="delete",
            close_response="cancel",
        )
        dialog.add_response("cancel", _("Cancel"))
        dialog.add_response("delete", _("Delete"))
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


class TrashItem(Adw.Bin):
    def __init__(self, task_widget, trash) -> None:
        super().__init__()
        self.task_widget = task_widget
        self.uid = task_widget.uid
        self.trash = trash
        self.trash_list = trash.trash_list
        self._build_ui()

    def _build_ui(self) -> None:
        self.add_css_class("card")
        row = Adw.ActionRow(
            title=self.task_widget.get_prop("text"),
            css_classes=["rounded-corners"],
            height_request=60,
            title_selectable=True,
        )
        self.task_widget.task_row.connect(
            "notify::title", lambda *_: row.set_title(self.task_widget.get_prop("text"))
        )
        self.task_widget.task_list.title.bind_property(
            "title",
            row,
            "subtitle",
            GObject.BindingFlags.SYNC_CREATE,
        )
        row.add_suffix(
            Button(
                icon_name="emblem-ok-symbolic",
                on_click=self.on_restore,
                tooltip_text=_("Restore"),
                valign="center",
                css_classes=["flat", "circular"],
            )
        )
        box = Gtk.ListBox(
            selection_mode=0,
            css_classes=["rounded-corners"],
            accessible_role=Gtk.AccessibleRole.PRESENTATION,
        )
        box.append(row)
        self.set_child(box)

    def on_restore(self, _) -> None:
        """Restore task"""

        Log.info(f"Restore task: {self.uid}")

        to_remove = []

        def restore_task(uid: str = self.uid) -> None:
            for item in get_children(self.trash_list):
                if item.task_widget.get_prop("uid") == uid:
                    item.task_widget.update_props(["trash"], [False])
                    item.task_widget.toggle_visibility(True)
                    to_remove.append(uid)
                    if puid := item.task_widget.get_prop("parent"):
                        item.task_widget.parent.expand(True)
                        item.task_widget.parent.update_status()
                        restore_task(puid)
                    break

        restore_task()

        for item in get_children(self.trash_list):
            if item.uid in to_remove:
                self.trash_list.remove(item)

        self.task_widget.task_list.update_status()
        self.trash.update_status()
