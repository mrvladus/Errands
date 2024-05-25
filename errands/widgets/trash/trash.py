# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton
from errands.widgets.shared.components.dialogs import ConfirmDialog
from errands.widgets.shared.components.header_bar import ErrandsHeaderBar
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.task import Task


class Trash(Adw.Bin):
    def __init__(self):
        super().__init__()
        Log.debug("Trash Page: Load")
        State.trash_page = self
        self.__build_ui()

    def __build_ui(self):
        # Status Page
        self.status_page = Adw.StatusPage(
            title=_("Empty Trash"),
            description=_("No deleted items"),
            icon_name="errands-trash-symbolic",
            vexpand=True,
            css_classes=["compact"],
        )

        # Clear button
        clear_btn: ErrandsButton = ErrandsButton(
            tooltip_text=_("Clear"),
            on_click=self.on_trash_clear,
            icon_name="errands-trash-symbolic",
            css_classes=["flat"],
            valign=Gtk.Align.CENTER,
        )
        self.status_page.bind_property(
            "visible",
            clear_btn,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        # Restore button
        restore_btn: ErrandsButton = ErrandsButton(
            tooltip_text=_("Restore"),
            on_click=self.on_trash_restore,
            icon_name="errands-restore-symbolic",
            css_classes=["flat"],
            valign=Gtk.Align.CENTER,
        )
        self.status_page.bind_property(
            "visible",
            restore_btn,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        # Trash List
        self.trash_list = Gtk.ListBox(
            selection_mode=Gtk.SelectionMode.NONE,
            margin_bottom=32,
            margin_end=12,
            margin_start=12,
            css_classes=["transparent"],
        )
        self.trash_list.bind_property(
            "visible",
            self.status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[
                    ErrandsHeaderBar(
                        start_children=[clear_btn],
                        end_children=[restore_btn],
                        title_widget=Adw.WindowTitle(title=_("Trash")),
                    )
                ],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    children=[
                        self.status_page,
                        Gtk.ScrolledWindow(
                            propagate_natural_height=True,
                            child=Adw.Clamp(
                                maximum_size=1000,
                                tightening_threshold=300,
                                child=self.trash_list,
                            ),
                        ),
                    ],
                ),
            )
        )

    @property
    def trash_items(self) -> list[TrashItem]:
        return get_children(self.trash_list)

    def add_trash_item(self, uid: str) -> None:
        self.trash_list.append(TrashItem(uid))
        self.status_page.set_visible(False)

    def update_ui(self):
        tasks_dicts: list[TaskData] = [
            t for t in UserData.get_tasks_as_dicts() if t.trash and not t.deleted
        ]
        tasks_uids: list[str] = [t.uid for t in tasks_dicts]

        # Add items
        items_uids = [t.task_dict.uid for t in self.trash_items]
        for t in tasks_dicts:
            if t.uid not in items_uids:
                self.add_trash_item(t)

        for item in self.trash_items:
            # Remove items
            if item.task_dict.uid not in tasks_uids:
                self.trash_list.remove(item)
                continue

            # Update title and subtitle
            item.update_ui()

        # Show status
        self.status_page.set_visible(len(self.trash_items) == 0)

    def on_trash_clear(self, *args) -> None:
        def __confirm(_, res) -> None:
            if res == "cancel":
                Log.debug("Trash: Clear cancelled")
                return

            Log.info("Trash: Clear")

            UserData.delete_tasks_from_trash()
            State.trash_sidebar_row.update_ui()
            State.tasks_page.update_ui()
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

        for task in self.trash_items:
            task.on_restore_btn_clicked(None)


class TrashItem(Adw.ActionRow):
    def __init__(self, task: TaskData) -> None:
        super().__init__()
        self.task_dict: TaskData = task
        self.__build_ui()

    def __build_ui(self) -> None:
        self.props.height_request = 60
        self.set_title_selectable(True)
        self.set_margin_top(6)
        self.set_margin_bottom(6)
        self.add_css_class("card")
        self.add_suffix(
            ErrandsButton(
                tooltip_text=_("Restore"),
                icon_name="errands-check-toggle-symbolic",
                valign=Gtk.Align.CENTER,
                css_classes=["circular", "flat"],
                on_click=self.on_restore_btn_clicked,
            )
        )

    def update_ui(self):
        self.set_title(self.task_dict.text)
        self.set_subtitle(UserData.get_list_prop(self.task_dict.list_uid, "name"))

    def on_restore_btn_clicked(self, _) -> None:
        """Restore task and its parents"""

        Log.info(f"Restore task: {self.task_dict.uid}")

        parents_uids: list[str] = UserData.get_parents_uids_tree(
            self.task_dict.list_uid, self.task_dict.uid
        )
        parents_uids.append(self.task_dict.uid)
        for uid in parents_uids:
            UserData.update_props(self.task_dict.list_uid, uid, ["trash"], [False])
            task: Task = State.get_task(self.task_dict.list_uid, uid)
            task.task_list.update_title()
            task.toggle_visibility(True)
            if isinstance(task.parent, Task):
                task.parent.update_title()
                task.parent.update_progress_bar()

        State.trash_sidebar_row.update_ui()
        State.tasks_page.update_ui()
