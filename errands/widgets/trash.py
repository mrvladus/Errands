# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING


if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.data import TaskData, TaskListData, UserData
from errands.lib.utils import get_children
from errands.widgets.components import Box, Button, ConfirmDialog
from gi.repository import Adw, Gtk, GObject, Gio  # type:ignore
from errands.lib.sync.sync import Sync
from errands.lib.logging import Log


class Trash(Adw.Bin):
    def __init__(self):
        super().__init__()
        self.window: Window = Gio.Application.get_default().get_active_window()
        self.stack: Adw.ViewStack = self.window.stack
        self._build_ui()

    def _build_ui(self) -> None:
        # Headerbar
        hb: Adw.HeaderBar = Adw.HeaderBar(
            title_widget=Adw.WindowTitle(title=_("Trash"))
        )
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

    @property
    def trash_items(self) -> list[TrashItem]:
        return get_children(self.trash_list)

    def add_trash_item(self, uid: str) -> None:
        self.trash_list.append(TrashItem(uid))
        self.status.set_visible(False)

    def on_trash_clear(self, *args) -> None:
        def __confirm(_, res) -> None:
            if res == "cancel":
                Log.debug("Trash: Clear cancelled")
                return

            Log.info("Trash: Clear")

            UserData.run_sql(
                f"""UPDATE tasks
                SET deleted = 1
                WHERE trash = 1""",
            )
            self.window.sidebar.trash_item.update_ui()
            # Sync
            Sync.sync()

        Log.debug("Trash: Show confirm dialog")

        ConfirmDialog(
            _("Tasks will be permanently deleted"),
            _("Delete"),
            Adw.ResponseAppearance.DESTRUCTIVE,
            __confirm,
        )

    def on_trash_restore(self, *args) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Trash: Restore")

        trash_dicts: list[TaskData] = [
            t for t in UserData.get_tasks_as_dicts() if t["trash"] and not t["deleted"]
        ]
        for task in trash_dicts:
            UserData.update_props(task["list_uid"], task["uid"], ["trash"], [False])

        self.window.sidebar.update_ui()

    def update_ui(self):
        tasks_dicts: list[TaskData] = [
            t for t in UserData.get_tasks_as_dicts() if t["trash"] and not t["deleted"]
        ]
        tasks_uids: list[str] = [t["uid"] for t in tasks_dicts]
        lists_dicts: list[TaskListData] = UserData.get_lists_as_dicts()

        # Add items
        items_uids = [t.task_dict["uid"] for t in self.trash_items]
        for t in tasks_dicts:
            if t["uid"] not in items_uids:
                self.add_trash_item(t)

        for item in self.trash_items:
            # Remove items
            if item.task_dict["uid"] not in tasks_uids:
                self.trash_list.remove(item)
                continue

            # Update title and subtitle
            task_dict = [t for t in tasks_dicts if t["uid"] == item.task_dict["uid"]][0]
            list_dict = [l for l in lists_dicts if l["uid"] == task_dict["list_uid"]][0]
            if item.row.get_title() != task_dict["text"]:
                item.row.set_title(task_dict["text"])
            if item.row.get_subtitle() != list_dict["name"]:
                item.row.set_subtitle(list_dict["name"])

        # Show status
        self.status.set_visible(len(self.trash_items) == 0)


class TrashItem(Adw.Bin):
    def __init__(self, task: TaskData) -> None:
        super().__init__()
        self.task_dict: TaskData = task
        self.window: Window = Gio.Application.get_default().get_active_window()
        self.__build_ui()

    def __build_ui(self) -> None:
        self.add_css_class("card")
        self.row: Adw.ActionRow = Adw.ActionRow(
            title=self.task_dict["text"],
            css_classes=["rounded-corners"],
            height_request=60,
            title_selectable=True,
        )
        self.row.add_suffix(
            Button(
                icon_name="emblem-ok-symbolic",
                on_click=self.on_restore,
                tooltip_text=_("Restore"),
                valign="center",
                css_classes=["flat", "circular"],
            )
        )
        box: Gtk.ListBox = Gtk.ListBox(
            selection_mode=0,
            css_classes=["rounded-corners"],
            accessible_role=Gtk.AccessibleRole.PRESENTATION,
        )
        box.append(self.row)
        self.set_child(box)

    def on_restore(self, _) -> None:
        """Restore task and its parents"""

        Log.info(f"Restore task: {self.task_dict['uid']}")

        parents_uids: list[str] = UserData.get_task_parents_uids_tree(
            self.task_dict["list_uid"], self.task_dict["uid"]
        )
        parents_uids.append(self.task_dict["uid"])
        for uid in parents_uids:
            UserData.update_props(self.task_dict["list_uid"], uid, ["trash"], [False])

        self.window.sidebar.update_ui()
