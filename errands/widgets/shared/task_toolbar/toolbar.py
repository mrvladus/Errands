# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from gi.repository import Adw, Gio, GLib, Gtk  # type:ignore

from errands.lib.utils import get_human_datetime
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton


if TYPE_CHECKING:
    from errands.widgets.task import Task


class ErrandsTaskToolbar(Gtk.FlowBox):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()

    def __build_ui(self) -> None:
        self.set_margin_bottom(2)
        self.set_margin_start(9)
        self.set_margin_end(9)
        self.set_max_children_per_line(2)
        self.set_selection_mode(Gtk.SelectionMode.NONE)

        # Date and Time button
        self.date_time_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            halign=Gtk.Align.START,
            hexpand=True,
            tooltip_text=_("Start / Due Date"),
            css_classes=["flat", "caption"],
            child=Adw.ButtonContent(
                icon_name="errands-calendar-symbolic",
                can_shrink=True,
                label=_("Date"),
            ),
            on_click=lambda *_: State.datetime_window.show(self.task),
        )
        self.append(self.date_time_btn)

        # Notes button
        self.notes_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),
            css_classes=["flat"],
            on_click=lambda *_: State.notes_window.show(self.task),
        )

        # Priority button
        self.priority_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-priority-symbolic",
            tooltip_text=_("Priority"),
            css_classes=["flat"],
            on_click=lambda *_: State.priority_window.show(self.task),
        )

        # Tags button
        tag_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-tag-add-symbolic",
            tooltip_text=_("Tags"),
            css_classes=["flat"],
            on_click=lambda *_: State.tag_window.show(self.task),
        )

        self.attachments_btn: ErrandsButton = ErrandsButton(
            tooltip_text=_("Attachments"),
            icon_name="errands-attachment-symbolic",
            css_classes=["flat"],
            on_click=lambda *_: State.attachments_window.show(self.task),
        )

        detail_btn: ErrandsButton = ErrandsButton(
            tooltip_text=_("Details"),
            icon_name="errands-more-symbolic",
            css_classes=["flat"],
            on_click=lambda *_: State.detail_window.show(self.task),
        )

        self.append(
            ErrandsBox(
                spacing=2,
                halign=Gtk.Align.END,
                children=[
                    self.notes_btn,
                    self.priority_btn,
                    tag_btn,
                    self.attachments_btn,
                    detail_btn,
                ],
            )
        )

    def update_ui(self):
        # Update Date and Time
        self.date_time_btn.get_child().props.label = get_human_datetime(
            self.task.task_data.due_date
        )
        self.date_time_btn.remove_css_class("error")
        if (
            self.task.task_data.due_date
            and datetime.fromisoformat(self.task.task_data.due_date).date()
            < datetime.today().date()
        ):
            self.date_time_btn.add_css_class("error")

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

        # Update attachments button css
        self.attachments_btn.remove_css_class("accent")
        if len(self.task.task_data.attachments) > 0:
            self.attachments_btn.add_css_class("accent")
