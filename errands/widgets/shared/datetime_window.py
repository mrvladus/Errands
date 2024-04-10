# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, Gtk  # type:ignore

from errands.state import State
from errands.widgets.shared.datetime_picker import DateTimePicker

if TYPE_CHECKING:
    from errands.widgets.task_py.task import Task
    from errands.widgets.today.today_task import TodayTask


class DateTimeWindow(Adw.Dialog):
    def __init__(self, task: Task | TodayTask):
        super().__init__()
        self.task = task
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_follows_content_size(True)
        self.set_title(_("Date and Time"))  # noqa: F821

        # View Stack
        stack: Adw.ViewStack = Adw.ViewStack()

        # Due Page
        self.due_date_time: DateTimePicker = DateTimePicker(
            margin_start=12, margin_end=12, margin_top=12, margin_bottom=12
        )
        stack.add_titled_with_icon(
            name=_("Due"),  # noqa: F821
            icon_name="errands-calendar-symbolic",
            title=_("Due"),  # noqa: F821
            child=Gtk.ScrolledWindow(
                propagate_natural_height=True,
                propagate_natural_width=True,
                child=self.due_date_time,
            ),
        )

        # Start Page
        self.start_date_time: DateTimePicker = DateTimePicker(
            margin_start=12, margin_end=12, margin_top=12, margin_bottom=12
        )
        stack.add_titled_with_icon(
            name=_("Start"),  # noqa: F821
            icon_name="errands-calendar-symbolic",
            title=_("Start"),  # noqa: F821
            child=Gtk.ScrolledWindow(
                propagate_natural_height=True,
                propagate_natural_width=True,
                child=self.start_date_time,
            ),
        )

        # Header Bar
        hb: Adw.HeaderBar = Adw.HeaderBar(
            title_widget=Adw.ViewSwitcher(
                policy=Adw.ViewSwitcherPolicy.WIDE, stack=stack
            )
        )

        # Toolbar View
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(content=stack)
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    # ------ PUBLIC METHODS ------ #

    def show(self):
        self.start_date_time.datetime = self.task.get_prop("start_date")
        self.due_date_time.datetime = self.task.get_prop("due_date")
        self.present(State.main_window)

    # ------ SIGNAL HANDLERS ------ #

    def do_closed(self):
        if self.due_date_time.datetime != self.task.get_prop("due_date"):
            self.task.update_props(
                ["due_date", "synced"], [self.due_date_time.datetime, False]
            )
            self.task.toolbar.update_ui()
        if self.start_date_time.datetime != self.task.get_prop("start_date"):
            self.task.update_props(
                ["start_date", "synced"], [self.start_date_time.datetime, False]
            )
