# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.sync.sync import Sync
from errands.widgets.components import Box
from errands.widgets.details import Details
from gi.repository import Adw, Gtk, GLib, GObject  # type:ignore
from errands.lib.animation import scroll
from errands.lib.data import TaskData, UserData
from errands.lib.utils import get_children
from errands.lib.logging import Log
from errands.widgets.task import Task
from errands.lib.gsettings import GSettings


class TaskListHeaderBar(Adw.Bin):

    # Public items
    title: Adw.WindowTitle

    def __init__(self, task_list: TaskList):
        super().__init__()
        self.task_list: TaskList = task_list
        self.__build_ui()

    def __build_ui(self) -> None:
        # Title
        self.title = Adw.WindowTitle()

        # Delete completed button
        self.delete_completed_btn = Gtk.Button(
            valign="center",
            icon_name="edit-clear-all-symbolic",
            tooltip_text=_("Delete Completed Tasks"),
            sensitive=False,
        )
        self.delete_completed_btn.connect(
            "clicked", self.on_delete_completed_btn_clicked
        )

        # Scroll up btn
        self.scroll_up_btn = Gtk.Button(
            valign="center",
            icon_name="go-up-symbolic",
            tooltip_text=_("Scroll Up"),
            visible=False,
        )
        self.scroll_up_btn.connect(
            "clicked", lambda *_: scroll(self.task_list.scrl, False)
        )

        # Headerbar
        hb: Adw.HeaderBar = Adw.HeaderBar()
        hb.set_title_widget(self.title)
        hb.pack_start(self.delete_completed_btn)
        hb.pack_end(self.scroll_up_btn)

        self.set_child(hb)

    def on_delete_completed_btn_clicked(self, _) -> None:
        """Hide completed tasks and move them to trash"""

        Log.info("Delete completed tasks")
        for task in self.task_list.all_tasks:
            if not task.get_prop("trash") and task.get_prop("completed"):
                task.delete()
        self.update_ui()

    def update_ui(self):
        # Rename list
        self.title.set_title(
            UserData.run_sql(
                f"""SELECT name FROM lists 
                WHERE uid = '{self.task_list.list_uid}'""",
                fetch=True,
            )[0][0]
        )

        n_total: int = 0
        n_completed: int = 0
        tasks: list[TaskData] = UserData.get_tasks_as_dicts(self.task_list.list_uid)
        for task in tasks:
            if not task["deleted"]:
                n_total += 1
                if task["completed"] and not task["trash"]:
                    n_completed += 1

        # Update status
        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}" if n_total > 0 else ""
        )

        # Update delete completed button
        self.delete_completed_btn.set_sensitive(n_completed > 0)


class TaskListEntry(Adw.Bin):

    # Public elements
    entry: Adw.EntryRow

    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list = task_list
        self.__build_ui()

    def __build_ui(self) -> None:
        self.entry = Adw.EntryRow(
            height_request=60,
            title=_("Add new Task"),
            css_classes=["card"],
            margin_top=6,
            margin_bottom=6,
            margin_end=12,
            margin_start=12,
        )
        self.entry.connect("entry-activated", self._on_task_added)

        self.set_child(
            Adw.Clamp(
                maximum_size=1000,
                tightening_threshold=300,
                child=self.entry,
            )
        )

    def _on_task_added(self, entry: Adw.EntryRow | Gtk.Entry) -> None:
        text: str = entry.get_text()
        if text.strip(" \n\t") == "":
            return
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        UserData.add_task(
            list_uid=self.task_list.list_uid,
            text=text,
            insert_at_the_top=on_top,
        )

        entry.set_text("")
        if not on_top:
            scroll(self.task_list.scrl, True)

        self.task_list.update_ui()
        Sync.sync()


class TaskListScrolledWindow(Gtk.ScrolledWindow):

    # State
    scrolling: bool = False

    def __init__(self, task_list: TaskList):
        super().__init__()
        self.task_list: TaskList = task_list
        self.__build_ui()

    def __build_ui(self) -> None:
        self.set_propagate_natural_height(True)
        self.set_propagate_natural_width(True)

        # Adjustment
        self.adj = Gtk.Adjustment()
        self.adj.connect(
            "value-changed",
            lambda *_: self.task_list.headerbar.scroll_up_btn.set_visible(
                self.adj.get_value() > 0
            ),
        )
        self.set_vadjustment(self.adj)

        self.dnd_ctrl = Gtk.DropControllerMotion()
        self.dnd_ctrl.connect("motion", self.__on_dnd_scroll)
        self.add_controller(self.dnd_ctrl)

    def __on_dnd_scroll(self, _motion, _x, y: float) -> bool:
        """Autoscroll while dragging task"""

        def __auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.dnd_ctrl.contains_pointer():
                return False
            self.adj.set_value(self.adj.get_value() - (2 if scroll_up else -2))
            return True

        MARGIN: int = 50
        if y < MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, __auto_scroll, True)
        elif y > self.get_allocation().height - MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, __auto_scroll, False)
        else:
            self.scrolling = False


class TaskList(Adw.Bin):

    # Public elements
    headerbar: TaskListHeaderBar
    scrl: TaskListScrolledWindow
    uncompleted_list: TaskListUncompletedList
    completed_list: TaskListCompletedList

    def __init__(self, window: Window, list_uid: str):
        super().__init__()
        self.window: Window = window
        self.list_uid: str = list_uid
        self.__build_ui()

    def __build_ui(self) -> None:
        # Header Bar
        self.headerbar = TaskListHeaderBar(self)

        # Un-Completed Tasks list
        self.uncompleted_list = TaskListUncompletedList(self)

        # Completed Tasks list
        self.completed_list = TaskListCompletedList(self)

        # Srolled window
        self.scrl = TaskListScrolledWindow(self)

        self.scrl.set_child(
            Adw.Clamp(
                maximum_size=1000,
                tightening_threshold=300,
                margin_bottom=32,
                child=Box(
                    children=[self.uncompleted_list, self.completed_list],
                    orientation=Gtk.Orientation.VERTICAL,
                ),
            )
        )

        # Content box
        content_box = Box(
            children=[TaskListEntry(self), self.scrl],
            orientation="vertical",
            vexpand=True,
        )

        # Tasks list toolbar view
        self.tasks_toolbar_view = Adw.ToolbarView(
            content=content_box,
            reveal_bottom_bars=False,
        )
        self.tasks_toolbar_view.add_top_bar(self.headerbar)
        self.set_child(self.tasks_toolbar_view)

    @property
    def all_tasks(self) -> list[Task]:
        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.completed_tasks.tasks)
                __add_task(task.uncompleted_tasks.tasks)

        __add_task(self.completed_list.tasks)
        __add_task(self.uncompleted_list.tasks)

        return all_tasks

    def update_ui(self) -> None:
        Log.debug(f"Task list {self.list_uid}: Update UI")

        self.uncompleted_list.update_ui()
        self.completed_list.update_ui()
        self.headerbar.update_ui()


class TaskListUncompletedList(Gtk.Box):
    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.list_uid: str = task_list.list_uid
        self.window: Window = task_list.window
        self.__build_ui()

    def __build_ui(self) -> None:
        self.set_orientation(Gtk.Orientation.VERTICAL)

    @property
    def tasks_dicts(self) -> list[TaskData]:
        return [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if not t["trash"]
            and not t["deleted"]
            and not t["completed"]
            and t["list_uid"] == self.list_uid
            and t["parent"] == ""
        ]

    @property
    def tasks(self) -> list[Task]:
        return get_children(self)

    def update_ui(self):
        data_uids: list[str] = [
            t["uid"]
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if t["parent"] == "" and not t["completed"] and not t["deleted"]
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        for uid in data_uids:
            if uid not in widgets_uids:
                new_task = Task(uid, self.task_list, self, False)
                if on_top:
                    self.prepend(new_task)
                else:
                    self.append(new_task)

        # Remove tasks
        for task in self.tasks:
            if task.uid not in data_uids:
                task.purged = True

        # Update tasks
        for task in self.tasks:
            task.update_ui()


class TaskListCompletedList(Gtk.Box):

    # Public items
    completed_list: Gtk.Box

    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.list_uid: str = task_list.list_uid
        self.window: Window = task_list.window
        self.__build_ui()

    def __build_ui(self) -> None:
        self.set_orientation(Gtk.Orientation.VERTICAL)
        GSettings.bind("show-completed-tasks", self, "visible")

        # Separator
        separator = Gtk.Box(
            css_classes=["dim-label"],
            margin_start=24,
            margin_end=24,
            margin_top=6,
            margin_bottom=6,
        )
        separator.append(Gtk.Separator(valign=Gtk.Align.CENTER, hexpand=True))
        separator.append(
            Gtk.Label(
                label=_("Completed Tasks"),
                css_classes=["caption-heading"],
                halign=Gtk.Align.CENTER,
                hexpand=False,
                margin_start=12,
                margin_end=12,
            )
        )
        separator.append(Gtk.Separator(valign=Gtk.Align.CENTER, hexpand=True))
        self.separator_rev: Gtk.Revealer = Gtk.Revealer(
            child=separator, transition_type=Gtk.RevealerTransitionType.CROSSFADE
        )
        self.append(self.separator_rev)

        # Completed tasks list
        self.completed_list = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.append(self.completed_list)

    @property
    def tasks_dicts(self) -> list[TaskData]:
        return [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if not t["trash"]
            and not t["deleted"]
            and t["completed"]
            and t["list_uid"] == self.list_uid
            and t["parent"] == ""
        ]

    @property
    def tasks(self) -> list[Task]:
        return get_children(self.completed_list)

    def update_ui(self) -> None:
        data_uids: list[str] = [
            t["uid"]
            for t in UserData.get_tasks_as_dicts(self.list_uid, "")
            if t["parent"] == "" and t["completed"] and not t["deleted"]
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for uid in data_uids:
            if uid not in widgets_uids:
                self.completed_list.prepend(Task(uid, self.task_list, self, False))

        # Remove tasks
        for task in self.tasks:
            if task.uid not in data_uids:
                task.purged = True

        # Update tasks
        for task in self.tasks:
            task.update_ui()

        # Show separator
        data: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid)
            if not t["trash"]
            and not t["deleted"]
            and t["completed"]
            and t["list_uid"] == self.list_uid
            and t["parent"] == ""
        ]
        self.separator_rev.set_reveal_child(len(data) > 0)
