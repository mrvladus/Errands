# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from datetime import datetime

from gi.repository import Adw, Gio, GObject, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsToggleButton
from errands.widgets.shared.components.header_bar import ErrandsHeaderBar
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView
from errands.widgets.tasks.task_data_list_view import TaskDataListView
from errands.widgets.tasks.tasks_page_name import TasksPageName
from errands.widgets.tasks.tasks_task import TasksTask


class TasksPage(TasksPageName, Adw.Bin):
    def __init__(self):
        super().__init__()
        Log.debug(f"{self.title} Page: Load")
        State.tasks_page = self
        self.filter_completed: bool = False
        self.filter_today: bool = True
        self.filter_text: str = ""
        self.sorter_priority: bool = True
        self.__build_ui()
        self.update_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        search_bar = Gtk.SearchBar()
        search_entry = Gtk.SearchEntry(
            hexpand=True,
            search_delay=100,
            margin_start=10,
            margin_end=10,
            placeholder_text=_("Search by text, notes or tags"),
        )
        search_entry.connect("search-changed", self._on_filter_text_entry)
        search_bar.set_child(
            Adw.Clamp(child=search_entry, maximum_size=1000, tightening_threshold=300)
        )
        search_bar.connect_entry(search_entry)
        search_bar.set_key_capture_widget(self)
        search_btn: Gtk.ToggleButton = Gtk.ToggleButton(
            icon_name="errands-search-symbolic", tooltip_text=_("Search and Filter")
        )
        search_btn.bind_property(
            "active",
            search_bar,
            "search-mode-enabled",
            GObject.BindingFlags.SYNC_CREATE,
        )

        # Status Page
        self.status_page = Adw.StatusPage(
            title=_("No Tasks"),
            icon_name="errands-info-symbolic",
            vexpand=True,
            css_classes=["compact"],
        )

        # Task List
        self.task_list = Gio.ListStore(item_type=TaskDataListView)
        self.task_list_filter = Gtk.CustomFilter.new(self.filter, None)
        task_list_filter_model = Gtk.FilterListModel(
            model=self.task_list, filter=self.task_list_filter
        )
        self.task_list_sorter = Gtk.CustomSorter.new(self.sort)
        task_list_sort_model = Gtk.SortListModel(
            model=task_list_filter_model, sorter=self.task_list_sorter
        )
        factory = Gtk.SignalListItemFactory()
        factory.connect(
            "setup", lambda factory, list_item: list_item.set_child(TasksTask(self))
        )
        factory.connect(
            "bind",
            lambda factory, list_item: list_item.get_child().set_data(
                list_item.get_item()
            ),
        )

        # Content
        content = Gtk.ScrolledWindow(
            propagate_natural_height=True,
            child=Adw.ClampScrollable(
                maximum_size=1000,
                tightening_threshold=300,
                child=Gtk.ListView(
                    model=Gtk.NoSelection(model=task_list_sort_model),
                    factory=factory,
                    css_classes=["transparent"],
                ),
            ),
        )

        self.status_page.bind_property(
            "visible",
            content,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        toggle_completed_btn: ErrandsToggleButton = ErrandsToggleButton(
            icon_name="errands-check-toggle-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Toggle Completed Tasks Filter"),
            on_toggle=self._on_toggle_completed_btn_toggled,
        )
        toggle_completed_btn.set_active(self.filter_completed)

        toggle_today_btn: ErrandsToggleButton = ErrandsToggleButton(
            icon_name="errands-calendar-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Toggle Today Tasks Filter"),
            on_toggle=self._on_toggle_today_btn_toggled,
        )
        toggle_today_btn.set_active(self.filter_today)

        toggle_priority_btn: ErrandsToggleButton = ErrandsToggleButton(
            icon_name="errands-priority-set-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Toggle Priority Tasks Sorter"),
            on_toggle=self._on_toggle_priority_btn_toggled,
        )
        toggle_priority_btn.set_active(self.sorter_priority)

        self.set_child(
            ErrandsToolbarView(
                top_bars=[
                    ErrandsBox(
                        orientation=Gtk.Orientation.VERTICAL,
                        children=[
                            ErrandsHeaderBar(
                                start_children=[
                                    toggle_completed_btn,
                                    toggle_today_btn,
                                    toggle_priority_btn,
                                ],
                                end_children=[search_btn],
                                title_widget=Adw.WindowTitle(title=_(self.title)),
                            ),
                            search_bar,
                        ],
                    )
                ],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    children=[self.status_page, content],
                ),
            )
        )

    # ------ PROPERTIES ------ #

    @property
    def tasks(self) -> list[TaskDataListView]:
        return [
            self.task_list.get_item(index)
            for index in range(self.task_list.get_n_items())
        ]

    @property
    def tasks_data(self) -> list[TaskData]:
        return [task for task in UserData.tasks if not task.deleted and not task.trash]

    # ------ PUBLIC METHODS ------ #

    def add_task(self, task: TaskDataListView) -> None:
        self.task_list.append(task)
        self.status_page.set_visible(False)

    def update_status(self):
        """Update status and counter"""

        total: int = len([task for task in self.tasks_data if not task.completed])
        self.status_page.set_visible(total == 0)
        State.tasks_sidebar_row.size_counter.set_label(str(total) if total > 0 else "")

    def update_ui(self):
        Log.debug(f"{self.title} Page: Update UI")
        tasks = self.tasks_data
        tasks_uids: list[str] = [t.uid for t in tasks]

        # Add tasks
        for task in map(TaskDataListView, tasks):
            if not self.task_list.find(task)[0]:
                self.add_task(task)

        # Remove tasks
        for task in self.tasks:
            if task.uid not in tasks_uids:
                found, position = self.task_list.find(task)
                if found:
                    self.task_list.remove(position)

        self.task_list_sorter.changed(Gtk.SorterChange.DIFFERENT)
        self.update_status()

    def _on_toggle_completed_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        self.filter_completed = btn.get_active()
        self.task_list_filter.changed(Gtk.FilterChange.DIFFERENT)

    def _on_toggle_today_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        self.filter_today = btn.get_active()
        self.task_list_filter.changed(Gtk.FilterChange.DIFFERENT)

    def _on_toggle_priority_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        self.sorter_priority = btn.get_active()
        self.task_list_sorter.changed(Gtk.SorterChange.DIFFERENT)

    def _on_filter_text_entry(self, entry: Gtk.SearchEntry) -> None:
        self.filter_text = entry.get_text().strip()
        self.task_list_filter.changed(Gtk.FilterChange.DIFFERENT)

    def filter(self, task, user_data) -> bool:
        return (
            self.filter_completed == task.completed
            and (
                not self.filter_today
                or (
                    task.due_date
                    and datetime.fromisoformat(task.due_date).date()
                    <= datetime.today().date()
                )
            )
            and (
                self.filter_text in task.text
                or self.filter_text in task.notes
                or self.filter_text in "".join(task.tags)
            )
        )

    def sort(
        self, task1: TaskDataListView, task2: TaskDataListView, user_data
    ) -> GObject.TYPE_INT:
        if self.sorter_priority:
            return (task1.priority if task1.priority else 10) - (
                task2.priority if task2.priority else 10
            )
        return 0
