# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.widgets.shared.components.button import ErrandsButton
from errands.widgets.shared.components.list_box import ErrandsListBox
from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow
from errands.widgets.shared.titled_separator import TitledSeparator

if TYPE_CHECKING:
    from errands.widgets.task_py.task import Task


class TaskToolbar(Gtk.Revealer):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.update_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        self.task.title.toolbar_toggle_btn.bind_property(
            "active",
            self,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )

        # Date and Time button
        self.date_time_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            halign=Gtk.Align.START,
            hexpand=True,
            tooltip_text=_("Start / Due Date"),  # noqa: F821
            css_classes=["flat", "caption"],
            child=Adw.ButtonContent(
                icon_name="errands-calendar-symbolic",
                can_shrink=True,
                label=_("Date"),  # noqa: F821
            ),
            on_click=self._on_date_time_btn_clicked,
        )
        self.datetime_window: DateTimeWindow = DateTimeWindow(self.task)

        # Notes button
        self.notes_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),  # noqa: F821
            css_classes=["flat"],
            on_click=self._on_notes_btn_clicked,
        )
        self.notes_window: NotesWindow = NotesWindow(self.task)

        # Priority button
        priority_box: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            margin_bottom=6,
            margin_top=6,
            margin_end=6,
            margin_start=6,
            spacing=3,
        )
        priority_box.append(
            ErrandsListBox(
                on_row_activated=self._on_priority_selected,
                selection_mode=Gtk.SelectionMode.NONE,
                children=[
                    Gtk.ListBoxRow(
                        css_classes=["error"],
                        child=Gtk.Label(label=_("High")),  # noqa: F821
                    ),
                    Gtk.ListBoxRow(
                        css_classes=["warning"],
                        child=Gtk.Label(label=_("Medium")),  # noqa: F821
                    ),
                    Gtk.ListBoxRow(
                        css_classes=["accent"],
                        child=Gtk.Label(label=_("Low")),  # noqa: F821
                    ),
                    Gtk.ListBoxRow(child=Gtk.Label(label=_("None"))),  # noqa: F821
                ],
            )
        )
        priority_box.append(TitledSeparator(title=_("Custom")))  # noqa: F821
        self.custom_priority_btn: Gtk.SpinButton = Gtk.SpinButton(
            valign=Gtk.Align.CENTER,
            adjustment=Gtk.Adjustment(upper=9, lower=0, step_increment=1),
        )
        priority_box.append(self.custom_priority_btn)
        self.priority_btn: Gtk.MenuButton = Gtk.MenuButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-priority-symbolic",
            tooltip_text=_("Priority"),  # noqa: F821
            css_classes=["flat"],
            popover=Gtk.Popover(css_classes=["menu"], child=priority_box),
        )
        self.priority_btn.connect("notify::active", self._on_priority_toggled)

        # Suffix Box
        suffix_box: Gtk.Box = Gtk.Box(spacing=3, halign=Gtk.Align.END)
        suffix_box.append(self.notes_btn)
        suffix_box.append(self.priority_btn)

        # Flow Box Container
        flow_box: Gtk.FlowBox = Gtk.FlowBox(
            margin_start=9,
            margin_end=9,
            margin_bottom=2,
            max_children_per_line=2,
            selection_mode=0,
        )
        flow_box.append(self.date_time_btn)
        flow_box.append(suffix_box)
        self.set_child(flow_box)

    # ------ PROPERTIES ------ #

    # ------ PUBLIC METHODS ------ #

    def update_ui(self) -> None:
        # Update Date and Time
        self.datetime_window.due_date_time.datetime = self.task.task_data.due_date
        self.date_time_btn.get_child().props.label = (
            f"{self.datetime_window.due_date_time.human_datetime}"
        )

        # Update notes button css
        if self.task.task_data.notes:
            self.notes_btn.add_css_class("accent")
        else:
            self.notes_btn.remove_css_class("accent")

        # Update priority button css
        priority: int = self.task.task_data.priority
        self.priority_btn.props.css_classes = ["flat"]
        if 0 < priority < 5:
            self.priority_btn.add_css_class("error")
        elif 4 < priority < 9:
            self.priority_btn.add_css_class("warning")
        elif priority == 9:
            self.priority_btn.add_css_class("accent")
        self.priority_btn.set_icon_name(
            f"errands-priority{'-set' if priority>0 else ''}-symbolic"
        )

    # ------ SIGNAL HANDLERS ------ #

    def _on_date_time_btn_clicked(self, btn: Gtk.Button) -> None:
        self.datetime_window.show()

    def _on_notes_btn_clicked(self, btn: Gtk.Button) -> None:
        self.notes_window.show()

    def _on_priority_toggled(self, btn: Gtk.MenuButton, *_) -> None:
        priority: int = self.task.task_data.priority
        if btn.get_active():
            self.custom_priority_btn.set_value(priority)
        else:
            new_priority: int = self.custom_priority_btn.get_value_as_int()
            if priority != new_priority:
                Log.debug(f"Task Toolbar: Set priority to '{new_priority}'")
                self.task.update_props(["priority", "synced"], [new_priority, False])
                self.update_ui()
                Sync.sync()

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
        self.priority_btn.popdown()
