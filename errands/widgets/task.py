# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import Any, TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList

from errands.widgets.components import Box
from gi.repository import Gtk, Adw, Gdk, GObject, GLib  # type:ignore
from errands.lib.sync.sync import Sync
from errands.lib.logging import Log
from errands.lib.data import TaskData, UserData
from errands.lib.markup import Markup
from errands.lib.utils import get_children, threaded
from errands.lib.gsettings import GSettings


class TaskTopDropArea(Gtk.Revealer):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self._build_ui()

    def _build_ui(self):
        self.set_transition_type(5)

        # Drop Image
        top_drop_img: Gtk.Image = Gtk.Image(
            icon_name="list-add-symbolic",
            hexpand=True,
            css_classes=["dim-label", "task-drop-area"],
        )
        top_drop_img_target: Gtk.DropTarget = Gtk.DropTarget.new(
            actions=Gdk.DragAction.MOVE, type=Task
        )
        top_drop_img_target.connect("drop", self._on_drop)
        top_drop_img.add_controller(top_drop_img_target)
        self.set_child(top_drop_img)

        # Drop controller
        drop_ctrl: Gtk.DropControllerMotion = Gtk.DropControllerMotion.new()
        drop_ctrl.bind_property(
            "contains-pointer", self, "reveal-child", GObject.BindingFlags.SYNC_CREATE
        )
        self.task.add_controller(drop_ctrl)

    def _on_drop(self, _drop, task: Task, _x, _y) -> bool:
        """
        When task is dropped on "+" area on top of task
        """

        UserData.move_task_to_list(
            task.uid,
            task.list_uid,
            self.task.list_uid,
            self.task.parent.uid if isinstance(self.task.parent, Task) else "",
            False,
        )
        UserData.move_task_before(self.task.list_uid, task.uid, self.task.uid)
        # If task has the same parent
        if task.parent == self.task.parent:
            # Move widget
            self.task.get_parent().reorder_child_after(task, self.task)
            self.task.get_parent().reorder_child_after(self.task, task)
            return True
        # Change parent if different parents
        UserData.update_props(
            self.task.list_uid,
            task.uid,
            ["parent", "synced"],
            [self.task.parent.uid if isinstance(self.task.parent, Task) else "", False],
        )
        # Add new task widget
        new_task: Task = Task(
            task.uid,
            self.task.task_list,
            self.task.parent,
            self.task.get_prop("parent") != None,
        )
        self.task.get_parent().append(new_task)
        self.task.get_parent().reorder_child_after(new_task, self.task)
        self.task.get_parent().reorder_child_after(self.task, new_task)
        new_task.toggle_visibility(True)
        # Toggle completion
        if not task.task_row.complete_btn.get_active():
            self.task.update_props(["completed", "synced"], [False, False])
            self.task.just_added = True
            self.task.task_row.complete_btn.set_active(False)
            self.task.just_added = False
            for parent in self.task.get_parents_tree():
                if parent.task_row.complete_btn.get_active():
                    parent.update_props(["completed", "synced"], [False, False])
                    parent.just_added = True
                    parent.task_row.complete_btn.set_active(False)
                    parent.just_added = False
        # Update status
        task.purge()
        self.task.task_list.update_ui()
        # Sync
        Sync.sync()

        return True


class TaskTitleRow(Gtk.Overlay):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self._build_ui()

    def _build_ui(self):
        # Task Title Row
        self.task_row = Adw.ActionRow(
            title=Markup.find_url(Markup.escape(self.task.get_prop("text"))),
            css_classes=["rounded-corners", "transparent"],
            height_request=60,
            accessible_role=Gtk.AccessibleRole.ROW,
            cursor=Gdk.Cursor.new_from_name("pointer"),
            use_markup=True,
        )
        self.add_rm_crossline(self.task.get_prop("completed"))

        # Drag controller
        task_row_drag_source: Gtk.DragSource = Gtk.DragSource.new()
        task_row_drag_source.set_actions(Gdk.DragAction.MOVE)
        task_row_drag_source.connect("prepare", self._on_drag_prepare)
        task_row_drag_source.connect("drag-begin", self._on_drag_begin)
        task_row_drag_source.connect("drag-cancel", self._on_drag_end)
        task_row_drag_source.connect("drag-end", self._on_drag_end)
        self.task_row.add_controller(task_row_drag_source)

        # Drop controller
        task_row_drop_target = Gtk.DropTarget.new(
            actions=Gdk.DragAction.MOVE, type=Task
        )
        task_row_drop_target.connect("drop", self._on_drop)
        self.task_row.add_controller(task_row_drop_target)

        # Prefix
        self.complete_btn = TaskCompleteButton(self.task, self.task_row)
        self.task_row.add_prefix(self.complete_btn)

        # Suffix
        self.suffix = TaskTitleRowSuffix(self.task)
        self.task_row.add_suffix(self.suffix)

        self.expand_indicator = Gtk.Image(
            icon_name="errands-up-symbolic",
            css_classes=["expand-indicator"],
            halign=Gtk.Align.END,
            margin_end=50,
        )
        expand_indicator_rev = Gtk.Revealer(
            child=self.expand_indicator,
            transition_type=1,
            reveal_child=False,
            can_target=False,
        )
        GSettings.bind("primary-action-show-sub-tasks", expand_indicator_rev, "visible")
        self.add_overlay(expand_indicator_rev)

        # Expand indicator hover controller
        hover_ctrl = Gtk.EventControllerMotion()
        hover_ctrl.bind_property(
            "contains-pointer",
            expand_indicator_rev,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE,
        )
        self.task_row.add_controller(hover_ctrl)

        # Click controller
        task_row_click_ctrl = Gtk.GestureClick.new()
        task_row_click_ctrl.connect(
            "released",
            lambda *_: (
                self.task.expand(not self.task.sub_tasks_revealer.get_child_revealed())
                if GSettings.get("primary-action-show-sub-tasks")
                else self.task.task_row.suffix.show_details()
            ),
        )
        self.task_row.add_controller(task_row_click_ctrl)

        box = Gtk.ListBox(
            selection_mode=Gtk.SelectionMode.NONE,
            css_classes=["rounded-corners", "transparent"],
            accessible_role=Gtk.AccessibleRole.PRESENTATION,
        )
        box.append(self.task_row)

        self.set_child(box)

    def add_rm_crossline(self, add: bool) -> None:
        if add:
            self.task_row.add_css_class("task-completed")
        else:
            self.task_row.remove_css_class("task-completed")

    def update_ui(self):
        # Update widget title crossline and completed toggle
        completed: bool = self.task.get_prop("completed")
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.task.just_added = True
            self.complete_btn.set_active(completed)
            self.task.just_added = False

        # Update subtitle
        total, completed = self.task.get_status()
        self.task_row.set_subtitle(
            _("Completed:") + f" {completed} / {total}" if total > 0 else ""
        )

    # --- DND --- #

    def _on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        # Bug workaround when task is not sensitive after short dnd
        for task in self.task.task_list.all_tasks:
            task.set_sensitive(True)
        self.task.set_sensitive(False)
        value: GObject.Value = GObject.Value(Task)
        value.set_object(self.task)
        return Gdk.ContentProvider.new_for_value(value)

    def _on_drag_begin(self, _, drag) -> bool:
        text: str = self.task.get_prop("text")
        icon: Gtk.DragIcon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(Gtk.Button(label=text if len(text) < 20 else f"{text[0:20]}..."))

    def _on_drag_end(self, *_) -> bool:
        self.task.set_sensitive(True)
        # KDE dnd bug workaround for issue #111
        for task in self.task.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

    def _on_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """

        if task == self.task or task.parent == self.task:
            return

        # Change parent
        UserData.move_task_to_list(
            task.uid,
            task.list_uid,
            self.task.list_uid,
            self.task.get_prop("uid"),
            False,
        )
        # Toggle completion
        if not task.task_row.complete_btn.get_active():
            self.task.update_props(["completed", "synced"], [False, False])
            # self.task.just_added = True
            # self.task.task_row.complete_btn.set_active(False)
            # self.task.just_added = False
            for parent in self.task.get_parents_tree():
                if parent.task_row.complete_btn.get_active():
                    parent.update_props(["completed", "synced"], [False, False])
                    # parent.just_added = True
                    # parent.task_row.complete_btn.set_active(False)
                    # parent.just_added = False
        if not self.task.get_prop("expanded"):
            self.task.expand(True)
        # Remove old task
        task.purged = True
        self.task.task_list.update_ui()
        # Sync
        Sync.sync()

        return True


class TaskCompleteButton(Gtk.CheckButton):
    def __init__(self, task: Task, parent: Adw.ActionRow) -> None:
        super().__init__()
        self.task: Task = task
        self.parent: Adw.ActionRow = parent
        self.__build_ui()

    def __build_ui(self) -> None:
        self.set_tooltip_text(_("Mark as Completed"))
        self.set_active(self.task.get_prop("completed"))
        self.set_valign(Gtk.Align.CENTER)
        self.add_css_class("selection-mode")
        self.connect("toggled", self.__on_toggle)

    def __on_toggle(self, btn: Gtk.CheckButton) -> None:
        Log.debug(f"Task '{self.task.uid}': Set completed to '{self.get_active()}'")

        self.task.task_row.add_rm_crossline(self.get_active())
        if self.task.just_added:
            return

        self.task.update_props(["completed", "synced"], [self.get_active(), False])
        sub_tasks: list[Task] = self.task.uncompleted_tasks.tasks
        parents: list[Task] = self.task.get_parents_tree()

        if self.get_active():
            # Complete all sub-tasks
            for sub in sub_tasks:
                sub.just_added = True
                sub.task_row.complete_btn.set_active(True)
                sub.just_added = False
                sub.update_props(["completed", "synced"], [True, False])
        else:
            # Uncomplete parent if sub-task is uncompleted
            for parent in parents:
                if parent.task_row.complete_btn.get_active():
                    parent.just_added = True
                    parent.task_row.complete_btn.set_active(False)
                    parent.just_added = False
                    parent.update_props(["completed", "synced"], [False, False])

        self.task.task_list.update_ui()
        Sync.sync()


class TaskTitleRowSuffix(Gtk.Box):

    # Public elements
    expand_btn: Gtk.Button
    details_btn: Gtk.Button

    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self.__build_ui()

    def __build_ui(self):
        # Expand button
        self.expand_btn = Gtk.Button(
            icon_name="errands-up-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Expand / Fold"),
            css_classes=["flat", "circular", "fade", "rotate"],
        )
        self.expand_btn.connect("clicked", self.__on_expand)
        GSettings.bind(
            "primary-action-show-sub-tasks", self.expand_btn, "visible", True
        )
        self.append(self.expand_btn)

        # Details button
        self.details_btn = Gtk.Button(
            icon_name="errands-info-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Details"),
            css_classes=["flat", "circular"],
        )
        self.details_btn.connect("clicked", self.show_details)
        GSettings.bind("primary-action-show-sub-tasks", self.details_btn, "visible")
        self.append(self.details_btn)

    def __on_expand(self, *args):
        self.task.expand(not self.task.sub_tasks_revealer.get_child_revealed())

    def show_details(self, *args):
        # Close details on second click
        if (
            self.task.details.parent == self.task
            and not self.task.details.status.get_visible()
            and self.task.task_list.split_view.get_show_sidebar()
        ):
            self.task.task_list.split_view.set_show_sidebar(False)
            return
        # Update details and show sidebar
        self.task.details.update_info(self.task)
        self.task.task_list.split_view.set_show_sidebar(True)


class TaskInfoBar(Gtk.Box):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self._build_ui()

    def _build_ui(self):
        self.set_orientation(Gtk.Orientation.VERTICAL)

        # Progress bar
        self.progress_bar = Gtk.ProgressBar(
            margin_end=12,
            margin_start=12,
            margin_bottom=6,
        )
        self.progress_bar.add_css_class("osd")
        self.progress_bar.add_css_class("dim-label")
        GSettings.bind("task-show-progressbar", self.progress_bar, "visible")
        self.progress_bar_rev = Gtk.Revealer(
            child=self.progress_bar, transition_duration=250
        )
        self.append(self.progress_bar_rev)

        # Info
        # self.due_date = Button(
        #     icon_name="errands-calendar-symbolic",
        #     label=_("Due"),
        #     css_classes=["task-toolbar-btn"],
        #     cursor=Gdk.Cursor(name="pointer"),
        # )
        # self.priority = Button(
        #     icon_name="errands-priority-symbolic",
        #     label=_("Priority"),
        #     css_classes=["task-toolbar-btn"],
        #     cursor=Gdk.Cursor(name="pointer"),
        # )
        # self.notes = Button(
        #     icon_name="errands-notes-symbolic",
        #     label=_("Notes"),
        #     css_classes=["task-toolbar-btn"],
        #     cursor=Gdk.Cursor(name="pointer"),
        # )
        # self.color = Button(
        #     icon_name="errands-color-symbolic",
        #     label=_("Color"),
        #     css_classes=["task-toolbar-btn"],
        #     cursor=Gdk.Cursor(name="pointer"),
        # )
        # self.tags = Gtk.Box(hexpand=True)
        # self.status_box = Box(
        #     children=[
        #         self.due_date,
        #         self.priority,
        #         self.tags,
        #         self.notes,
        #         self.color,
        #     ],
        #     margin_end=12,
        #     margin_start=12,
        #     margin_bottom=3,
        # )
        # GSettings.bind("task-show-toolbar", self.status_box, "visible")
        # self.append(self.status_box)

    def update_ui(self):
        # end_date = self.task.get_prop("end_date")
        # start_date = self.task.get_prop("start_date")
        # notes = self.task.get_prop("notes")

        # Update percrent complete
        if GSettings.get("task-show-progressbar"):
            total, completed = self.task.get_status()
            pc: int = (
                completed / total * 100
                if total > 0
                else (100 if self.task.task_row.complete_btn.get_active() else 0)
            )
            if self.task.get_prop("percent_complete") != pc:
                self.task.update_props(["percent_complete", "synced"], [pc, False])

            self.progress_bar.set_fraction(pc / 100)
            self.progress_bar_rev.set_reveal_child(self.task.get_status()[0] > 0)


class TaskSubTasksEntry(Gtk.Entry):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self._build_ui()

    def _build_ui(self) -> None:
        self.set_placeholder_text(_("Add new Sub-Task"))
        self.set_hexpand(True)
        self.set_margin_bottom(2)
        self.set_margin_end(12)
        self.set_margin_start(12)

    def do_activate(self) -> None:
        text: str = self.get_text()

        # Return if entry is empty
        if text.strip(" \n\t") == "":
            return

        # Add sub-task
        UserData.add_task(
            list_uid=self.task.list_uid,
            text=text,
            parent=self.task.uid,
            insert_at_the_top=GSettings.get("task-list-new-task-position-top"),
        )

        # Clear entry
        self.set_text("")

        # Update status
        self.task.update_props(["completed", "synced"], [False, False])
        self.task.just_added = True
        self.task.task_row.complete_btn.set_active(False)
        self.task.just_added = False
        self.task.update_ui()

        # Sync
        Sync.sync()


class TaskUncompletedSubTasks(Gtk.Box):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self._build_ui()

    def _build_ui(self):
        self.set_orientation(Gtk.Orientation.VERTICAL)

    @property
    def tasks(self) -> list[Task]:
        return get_children(self)

    @property
    def tasks_dicts(self) -> list[TaskData]:
        return [
            t
            for t in UserData.get_tasks_as_dicts(self.task.list_uid)
            if not t["trash"]
            and not t["deleted"]
            and not t["completed"]
            and t["list_uid"] == self.task.list_uid
            and t["parent"] == self.task.uid
        ]

    def update_ui(self):
        data_uids: list[str] = [
            t["uid"]
            for t in UserData.get_tasks_as_dicts(self.task.list_uid, self.task.uid)
            if not t["deleted"]
            and not t["trash"]
            and not t["completed"]
            and t["parent"] == self.task.uid
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add sub-tasks
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        for uid in data_uids:
            if uid not in widgets_uids:
                task: Task = Task(uid, self.task.task_list, self.task, True)
                if on_top:
                    self.prepend(task)
                else:
                    self.append(task)

        # Remove sub-tasks
        for task in self.tasks:
            if task.uid not in data_uids:
                task.purged = True

        # Update sub-tasks
        for task in self.tasks:
            task.update_ui()


class TaskCompletedSubTasks(Gtk.Box):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self._build_ui()

    def _build_ui(self):
        self.set_orientation(Gtk.Orientation.VERTICAL)
        GSettings.bind("show-completed-tasks", self, "visible")

        # Separator
        separator = Gtk.Box(css_classes=["dim-label"], margin_start=20, margin_end=20)
        separator.append(Gtk.Separator(valign=Gtk.Align.CENTER, hexpand=True))
        separator.append(
            Gtk.Label(
                label=_("Completed Tasks"),
                css_classes=["caption"],
                halign=Gtk.Align.CENTER,
                hexpand=False,
                margin_start=12,
                margin_end=12,
            )
        )
        separator.append(Gtk.Separator(valign=Gtk.Align.CENTER, hexpand=True))
        self.separator_rev: Gtk.Revealer = Gtk.Revealer(
            child=separator, transition_type=Gtk.RevealerTransitionType.SWING_DOWN
        )
        self.append(self.separator_rev)

        # Completed tasks list
        self.completed_list = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.append(self.completed_list)

    @property
    def tasks(self) -> list[Task]:
        return get_children(self.completed_list)

    @property
    def tasks_dicts(self) -> list[TaskData]:
        return [
            t
            for t in UserData.get_tasks_as_dicts(self.task.list_uid)
            if not t["trash"]
            and not t["deleted"]
            and t["completed"]
            and t["list_uid"] == self.task.list_uid
            and t["parent"] == self.task.uid
        ]

    def update_ui(self):
        data_uids: list[str] = [
            t["uid"]
            for t in UserData.get_tasks_as_dicts(self.task.list_uid)
            if not t["deleted"]
            and not t["trash"]
            and t["completed"]
            and t["parent"] == self.task.uid
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add sub-tasks
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        for uid in data_uids:
            if uid not in widgets_uids:
                task: Task = Task(uid, self.task.task_list, self.task, True)
                if on_top:
                    self.completed_list.prepend(task)
                else:
                    self.completed_list.append(task)

        # Remove sub-tasks
        for task in self.tasks:
            if task.uid not in data_uids:
                task.purged = True

        # Update sub-tasks
        for task in self.tasks:
            task.update_ui()

        # Show separator
        self.separator_rev.set_reveal_child(
            len(self.tasks_dicts) > 0
            and len(self.task.uncompleted_tasks.tasks_dicts) > 0
        )


class Task(Gtk.Revealer):

    # Public elements
    top_drop_area: TaskTopDropArea
    task_row: TaskTitleRow
    info_bar: TaskInfoBar
    sub_tasks_entry: TaskSubTasksEntry
    uncompleted_tasks: TaskUncompletedSubTasks
    completed_tasks: TaskCompletedSubTasks
    sub_tasks_revealer: Gtk.Revealer
    main_box: Gtk.Box

    # State
    just_added: bool = True
    can_sync: bool = True
    purged: bool = False
    purging: bool = False

    def __init__(
        self,
        uid: str,
        task_list: TaskList,
        parent: TaskList | Task,
        is_sub_task: bool,
    ) -> None:
        super().__init__()
        Log.info(f"Add task: {uid}")

        self.uid = uid
        self.task_list = task_list
        self.list_uid = task_list.list_uid
        self.window = task_list.window
        self.parent = parent
        self.is_sub_task = is_sub_task
        self.details = task_list.details

        self._build_ui()
        self.just_added = False

    def _build_ui(self) -> None:
        self.set_transition_type(Gtk.RevealerTransitionType.SLIDE_DOWN)
        self.set_transition_duration(200)
        # Top drop area
        self.top_drop_area = TaskTopDropArea(self)

        # Task row
        self.task_row = TaskTitleRow(self)

        # Info bar
        self.info_bar = TaskInfoBar(self)

        # Sub-tasks
        self.sub_tasks_entry = TaskSubTasksEntry(self)

        # Uncompleted tasks
        self.uncompleted_tasks = TaskUncompletedSubTasks(self)

        # Completed tasks
        self.completed_tasks = TaskCompletedSubTasks(self)

        # Sub-tasks revealer
        self.sub_tasks_revealer = Gtk.Revealer(
            child=Box(
                children=[
                    self.sub_tasks_entry,
                    self.uncompleted_tasks,
                    self.completed_tasks,
                ],
                orientation=Gtk.Orientation.VERTICAL,
                css_classes=["sub-tasks"],
            )
        )

        # Task card
        self.main_box = Box(
            children=[self.task_row, self.info_bar, self.sub_tasks_revealer],
            orientation="vertical",
            hexpand=True,
            css_classes=["fade", "card", f'task-{self.get_prop("color")}'],
        )

        self.set_child(
            Box(
                children=[self.top_drop_area, self.main_box],
                orientation="vertical",
                margin_start=12,
                margin_end=12,
                margin_bottom=6,
                margin_top=6,
            )
        )

    @property
    def all_tasks(self) -> list[Task]:
        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.completed_tasks.tasks)
                __add_task(task.uncompleted_tasks.tasks)

        __add_task(self.completed_tasks.tasks)
        __add_task(self.uncompleted_tasks.tasks)
        return all_tasks

    def get_parents_tree(self) -> list[Task]:
        """Get parent tasks chain"""

        parents: list[Task] = []

        def _add(task: Task):
            if isinstance(task.parent, Task):
                parents.append(task.parent)
                _add(task.parent)

        _add(self)

        return parents

    def get_prop(self, prop: str) -> Any:
        res: Any = UserData.get_prop(self.list_uid, self.uid, prop)
        if prop in "deleted completed expanded trash":
            res = bool(res)
        return res

    def get_status(self) -> tuple[int, int]:
        """Get total tasks and completed tasks tuple"""

        tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.task_list.list_uid)
            if t["parent"] == self.uid and not t["deleted"] and not t["trash"]
        ]
        n_total: int = len(tasks)
        n_completed: int = len([t for t in tasks if t["completed"]])

        return n_total, n_completed

    def delete(self, *_) -> None:
        """Move task to trash"""

        Log.info(f"Task: Move to trash: '{self.uid}'")

        self.toggle_visibility(False)
        self.task_row.complete_btn.set_active(True)
        self.update_props(["trash", "synced"], [True, False])
        for task in self.all_tasks:
            task.delete()
        self.parent.update_ui()
        self.window.sidebar.trash_item.update_ui()

    def expand(self, expanded: bool) -> None:
        self.sub_tasks_revealer.set_reveal_child(expanded)
        self.update_props(["expanded"], [expanded])
        if expanded:
            self.task_row.suffix.expand_btn.remove_css_class("rotate")
            self.task_row.expand_indicator.remove_css_class("expand-indicator-expanded")
        else:
            self.task_row.suffix.expand_btn.add_css_class("rotate")
            self.task_row.expand_indicator.add_css_class("expand-indicator-expanded")

    def purge(self) -> None:
        """Completely remove widget"""

        if self.purging:
            return

        def __finish_remove():
            GLib.idle_add(self.get_parent().remove, self)
            return False

        self.purging = True
        self.toggle_visibility(False)
        GLib.timeout_add(200, __finish_remove)

    def toggle_visibility(self, on: bool) -> None:
        GLib.idle_add(self.set_reveal_child, on)

    def update_props(self, props: list[str], values: list[Any]) -> None:
        UserData.update_props(self.list_uid, self.uid, props, values)

    def update_ui(self) -> None:
        if self.purged:
            self.purge()
            return

        # Change visibility
        self.toggle_visibility(not self.get_prop("trash"))

        # Expand
        self.expand(self.get_prop("expanded"))

        self.task_row.update_ui()
        self.info_bar.update_ui()
        self.completed_tasks.update_ui()
        self.uncompleted_tasks.update_ui()
