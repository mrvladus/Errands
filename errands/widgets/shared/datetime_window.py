# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.state import State
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.shared.datetime_picker import DateTimePicker

if TYPE_CHECKING:
    from errands.widgets.task import Task
    from errands.widgets.today.today_task import TodayTask


class DateTimeWindow(Adw.Dialog):
    date_time_set = GObject.Signal(name="date-time-set")

    def __init__(self, task: Task | TodayTask):
        super().__init__()
        self.task = task
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_follows_content_size(True)
        self.set_title(_("Date and Time"))

        # View Stack
        stack: Adw.ViewStack = Adw.ViewStack()

        # Due Page
        self.due_date_time: DateTimePicker = DateTimePicker(
            margin_start=12, margin_end=12, margin_top=12, margin_bottom=12
        )
        stack.add_titled_with_icon(
            name=_("Due"),
            icon_name="errands-calendar-symbolic",
            title=_("Due"),
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
            name=_("Start"),
            icon_name="errands-calendar-symbolic",
            title=_("Start"),
            child=Gtk.ScrolledWindow(
                propagate_natural_height=True,
                propagate_natural_width=True,
                child=self.start_date_time,
            ),
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[
                    Adw.HeaderBar(
                        title_widget=Adw.ViewSwitcher(
                            policy=Adw.ViewSwitcherPolicy.WIDE, stack=stack
                        )
                    )
                ],
                content=stack,
            )
        )

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
            self.task.update_toolbar()
        if self.start_date_time.datetime != self.task.get_prop("start_date"):
            self.task.update_props(
                ["start_date", "synced"], [self.start_date_time.datetime, False]
            )
        self.emit("date-time-set")
