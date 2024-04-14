# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from datetime import datetime

from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.today.today_task import TodayTask


class Today(Adw.Bin):
    def __init__(self):
        super().__init__()
        Log.debug("Today Page: Load")
        State.today_page = self
        self.__build_ui()
        self.update_status()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        # Status Page
        self.status_page = Adw.StatusPage(
            title=_("No Tasks for Today"),
            description=_("No deleted items"),
            icon_name="errands-info-symbolic",
            vexpand=True,
            css_classes=["compact"],
        )

        # Task List
        self.task_list = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, margin_bottom=32)

        # Content
        content = Gtk.ScrolledWindow(
            propagate_natural_height=True,
            child=Adw.Clamp(
                maximum_size=1000, tightening_threshold=300, child=self.task_list
            ),
        )
        self.status_page.bind_property(
            "visible",
            content,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[
                    Adw.HeaderBar(title_widget=Adw.WindowTitle(title=_("Today")))
                ],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    children=[self.status_page, content],
                ),
            )
        )

    # ------ PROPERTIES ------ #

    @property
    def tasks(self) -> list[TodayTask]:
        return get_children(self.task_list)

    @property
    def tasks_data(self) -> list[TaskData]:
        return [
            t
            for t in UserData.tasks
            if not t.deleted
            and not t.trash
            and t.due_date
            and datetime.fromisoformat(t.due_date).date() == datetime.today().date()
        ]

    # ------ PUBLIC METHODS ------ #

    def add_task(self, task) -> TodayTask:
        new_task = TodayTask(task)
        self.task_list.append(new_task)
        self.status_page.set_visible(False)
        return new_task

    def update_status(self):
        """Update status and counter"""

        length: int = len(self.tasks_data)
        self.status_page.set_visible(length == 0)
        State.today_sidebar_row.size_counter.set_label(
            "" if length == 0 else str(length)
        )

    def update_ui(self):
        return
        Log.debug("Today Page: Update UI")
        tasks = self.tasks_data
        tasks_uids: list[str] = [t.uid for t in tasks]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for task in tasks:
            if task.uid not in widgets_uids:
                new_task = TodayTask(task)
                self.task_list.append(new_task)
                new_task.update_ui()

        # Remove tasks
        for task in self.tasks:
            if task.uid not in tasks_uids:
                task.purge()

        for task in self.tasks:
            task.update_ui()

        self.update_status()
