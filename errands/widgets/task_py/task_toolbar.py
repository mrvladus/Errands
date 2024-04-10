# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow

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
        self.date_time_btn: Gtk.Button = Gtk.Button(
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
        )
        self.date_time_btn.connect("clicked", self._on_date_time_btn_clicked)
        self.datetime_window: DateTimeWindow = DateTimeWindow(self.task)

        # Notes button
        self.notes_btn: Gtk.Button = Gtk.Button(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),  # noqa: F821
            css_classes=["flat"],
        )
        self.notes_btn.connect("clicked", self._on_notes_btn_clicked)
        self.notes_window: NotesWindow = NotesWindow(self.task)

        # Suffix Box
        suffix_box: Gtk.Box = Gtk.Box(spacing=3, halign=Gtk.Align.END)
        suffix_box.append(self.notes_btn)

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

    # ------ SIGNAL HANDLERS ------ #

    def _on_date_time_btn_clicked(self, btn: Gtk.Button) -> None:
        self.datetime_window.show()

    def _on_notes_btn_clicked(self, btn: Gtk.Button) -> None:
        self.notes_window.show()
