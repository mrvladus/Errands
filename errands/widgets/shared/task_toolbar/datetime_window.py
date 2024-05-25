# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.shared.datetime_picker import DateTimePicker

if TYPE_CHECKING:
    from errands.widgets.task import Task
    from errands.widgets.tasks.tasks_task import TasksTask


class ErrandsDateTimeWindow(Adw.Dialog):
    def __init__(self):
        super().__init__()
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

    def show(self, task: Task | TasksTask):
        self.task = task
        self.start_date_time.datetime = self.task.task_data.start_date
        self.due_date_time.datetime = self.task.task_data.due_date
        self.present(State.main_window)

    # ------ SIGNAL HANDLERS ------ #

    def do_closed(self):
        changed: bool = False
        if self.due_date_time.datetime != self.task.task_data.due_date:
            self.task.update_props(
                ["due_date", "synced"], [self.due_date_time.datetime, False]
            )
            self.task.update_toolbar()
            changed = True
        if self.start_date_time.datetime != self.task.task_data.start_date:
            self.task.update_props(
                ["start_date", "synced"], [self.start_date_time.datetime, False]
            )
            changed = True

        State.tasks_page.update_ui()
        if changed:
            Sync.sync()
