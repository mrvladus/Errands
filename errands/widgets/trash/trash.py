# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.data import TaskData, TaskListData, UserData
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.component import ConfirmDialog
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton
from errands.widgets.trash.trash_item import TrashItem


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

        # Header Bar
        hb: Adw.HeaderBar = Adw.HeaderBar(
            title_widget=Adw.WindowTitle(title=_("Trash"))
        )

        # Clear button
        clear_button: ErrandsButton = ErrandsButton(
            on_click=self._on_trash_clear,
            icon_name="errands-trash-symbolic",
            css_classes=["flat"],
        )
        self.status_page.bind_property(
            "visible",
            clear_button,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        hb.pack_start(clear_button)

        # Trash List
        self.trash_list = Gtk.ListBox(
            selection_mode=Gtk.SelectionMode.NONE,
            margin_bottom=32,
            css_classes=["transparent"],
        )
        self.trash_list.bind_property(
            "visible",
            self.status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        # Toolbar View
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(
            content=ErrandsBox(
                orientation=Gtk.Orientation.VERTICAL,
                children=[self.status_page, self.trash_list],
            )
        )
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

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
        lists_dicts: list[TaskListData] = UserData.get_lists_as_dicts()

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
            task_dict = [t for t in tasks_dicts if t.uid == item.task_dict.uid][0]
            list_dict = [l for l in lists_dicts if l.uid == task_dict.list_uid][0]
            if item.get_title() != task_dict.text:
                item.set_title(task_dict.text)
            if item.get_subtitle() != list_dict.name:
                item.set_subtitle(list_dict.name)

        # Show status
        self.status_page.set_visible(len(self.trash_items) == 0)

    def _on_trash_clear(self, *args) -> None:
        def __confirm(_, res) -> None:
            if res == "cancel":
                Log.debug("Trash: Clear cancelled")
                return

            Log.info("Trash: Clear")

            UserData.delete_tasks_from_trash()
            State.trash_sidebar_row.update_ui()
            # Sync
            Sync.sync()

        Log.debug("Trash: Show confirm dialog")

        ConfirmDialog(
            _("Tasks will be permanently deleted"),
            _("Delete"),
            Adw.ResponseAppearance.DESTRUCTIVE,
            __confirm,
        )

    def _on_trash_restore(self, *args) -> None:
        """
        Remove trash items and restore all tasks
        """

        Log.info("Trash: Restore")

        trash_dicts: list[TaskData] = [
            t for t in UserData.get_tasks_as_dicts() if t.trash and not t.deleted
        ]
        for task in trash_dicts:
            UserData.update_props(task.list_uid, task.uid, ["trash"], [False])

        State.sidebar.update_ui()
