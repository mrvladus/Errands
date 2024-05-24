# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.lib.utils import get_children
from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox, ErrandsListBox
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.shared.titled_separator import TitledSeparator

if TYPE_CHECKING:
    from errands.widgets.task import Task
    from errands.widgets.page_task import PageTask


class PriorityWindow(Adw.Dialog):
    def __init__(self):
        super().__init__()
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_follows_content_size(True)
        self.set_title(_("Priority"))

        self.custom_priority_btn: Gtk.SpinButton = Gtk.SpinButton(
            valign=Gtk.Align.CENTER,
            adjustment=Gtk.Adjustment(upper=9, lower=0, step_increment=1),
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[Adw.HeaderBar()],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    margin_bottom=6,
                    margin_top=6,
                    margin_end=6,
                    margin_start=6,
                    spacing=3,
                    children=[
                        ErrandsListBox(
                            on_row_activated=self._on_priority_selected,
                            selection_mode=Gtk.SelectionMode.NONE,
                            css_classes=["navigation-sidebar"],
                            children=[
                                Gtk.ListBoxRow(
                                    css_classes=["error"],
                                    child=Gtk.Label(label=_("High")),
                                ),
                                Gtk.ListBoxRow(
                                    css_classes=["warning"],
                                    child=Gtk.Label(label=_("Medium")),
                                ),
                                Gtk.ListBoxRow(
                                    css_classes=["accent"],
                                    child=Gtk.Label(label=_("Low")),
                                ),
                                Gtk.ListBoxRow(child=Gtk.Label(label=_("None"))),
                            ],
                        ),
                        TitledSeparator(title=_("Custom")),
                        self.custom_priority_btn,
                    ],
                ),
            )
        )

    def _on_priority_selected(self, box: Gtk.ListBox, row: Gtk.ListBoxRow) -> None:
        rows: list[Gtk.ListBoxRow] = get_children(box)
        for i, r in enumerate(rows):
            if r == row:
                index = i
                break
        match index:
            case 0:
                self.custom_priority_btn.set_value(1)
            case 1:
                self.custom_priority_btn.set_value(5)
            case 2:
                self.custom_priority_btn.set_value(9)
            case 3:
                self.custom_priority_btn.set_value(0)
        self.close()

    # ------ PUBLIC METHODS ------ #

    def show(self, task: Task | TodayTask):
        self.task = task
        self.custom_priority_btn.set_value(self.task.task_data.priority)
        self.present(State.main_window)

    # ------ SIGNAL HANDLERS ------ #

    def do_closed(self):
        priority: int = self.custom_priority_btn.get_value_as_int()
        if priority == self.task.task_data.priority:
            return
        Log.info("Task: Change priority")
        self.task.update_props(["priority", "synced"], [priority, False])
        self.task.update_toolbar()
        Sync.sync()
