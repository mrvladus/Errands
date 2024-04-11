# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow

# from errands.widgets.task.tag import Tag
from errands.widgets.task.tags_list_item import TagsListItem

if TYPE_CHECKING:
    from errands.widgets.task_list.task_list import TaskList

import os
from datetime import datetime
from icalendar import Calendar, Event

from gi.repository import Adw  # type:ignore
from gi.repository import Gdk  # type:ignore
from gi.repository import Gio  # type:ignore
from gi.repository import Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log

# from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Task(Adw.Bin):
    __gtype_name__ = "Task"

    revealer: Gtk.Revealer = Gtk.Template.Child()
    top_drop_area: Gtk.Revealer = Gtk.Template.Child()
    main_box: Gtk.Box = Gtk.Template.Child()
    progress_bar_rev: Gtk.Revealer = Gtk.Template.Child()
    progress_bar: Gtk.ProgressBar = Gtk.Template.Child()
    sub_tasks_revealer: Gtk.Revealer = Gtk.Template.Child()
    title_row: Adw.ActionRow = Gtk.Template.Child()
    complete_btn: Gtk.CheckButton = Gtk.Template.Child()
    expand_indicator: Gtk.Image = Gtk.Template.Child()
    entry_row: Adw.EntryRow = Gtk.Template.Child()
    tags_bar: Gtk.FlowBox = Gtk.Template.Child()
    tags_bar_rev: Gtk.Revealer = Gtk.Template.Child()
    toolbar: Gtk.Revealer = Gtk.Template.Child()
    uncompleted_tasks_list: Gtk.Box = Gtk.Template.Child()
    completed_tasks_list: Gtk.Box = Gtk.Template.Child()
    notes_btn: Gtk.MenuButton = Gtk.Template.Child()
    priority_btn: Gtk.MenuButton = Gtk.Template.Child()
    created_label: Gtk.Label = Gtk.Template.Child()
    changed_label: Gtk.Label = Gtk.Template.Child()
    date_time_btn: Gtk.MenuButton = Gtk.Template.Child()
    tags_list: Gtk.ListBox = Gtk.Template.Child()
    priority: Gtk.SpinButton = Gtk.Template.Child()
    accent_color_btns: Gtk.Box = Gtk.Template.Child()

    # State
    just_added: bool = True
    can_sync: bool = True
    purging: bool = False

    def __init__(
        self,
        task_data: TaskData,
        task_list: TaskList,
        parent: TaskList | Task,
    ) -> None:
        super().__init__()
        self.task_data = task_data
        self.uid = task_data.uid
        self.list_uid = task_data.list_uid
        self.task_list = task_list
        self.parent = parent
        self.notes_window: NotesWindow = NotesWindow(self)
        self.datetime_window: DateTimeWindow = DateTimeWindow(self)
        GSettings.bind("task-show-progressbar", self.progress_bar_rev, "visible")
        self.__add_actions()
        self.__load_sub_tasks()
        self.just_added = False

    def __repr__(self) -> str:
        return f"<class 'Task' {self.uid}>"

    def __add_actions(self) -> None:
        self.group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="task", group=self.group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction(name=name)
            action.connect("activate", callback)
            self.group.add_action(action)

        def __edit(*args):
            self.entry_row.set_text(self.get_prop("text"))
            self.entry_row.set_visible(True)

        def __export(*args):
            def __confirm(dialog, res):
                try:
                    file = dialog.save_finish(res)
                except Exception as e:
                    Log.debug(f"List: Export cancelled. {e}")
                    return

                Log.info(f"Task: Export '{self.uid}'")

                task = [
                    i
                    for i in UserData.get_tasks_as_dicts(self.list_uid)
                    if i.uid == self.uid
                ][0]
                calendar = Calendar()
                event = Event()
                event.add("uid", task.uid)
                event.add("summary", task.text)
                if task.notes:
                    event.add("description", task.notes)
                event.add("priority", task.priority)
                if task.tags:
                    event.add("categories", task.tags)
                event.add("percent-complete", task.percent_complete)
                if task.color:
                    event.add("x-errands-color", task.color)
                event.add(
                    "dtstart",
                    (
                        datetime.fromisoformat(task.start_date)
                        if task.start_date
                        else datetime.now()
                    ),
                )
                if task.due_date:
                    event.add("dtend", datetime.fromisoformat(task.due_date))
                calendar.add_component(event)

                with open(file.get_path(), "wb") as f:
                    f.write(calendar.to_ical())
                State.main_window.add_toast(_("Exported"))  # noqa: F821

            dialog = Gtk.FileDialog(initial_name=f"{self.uid}.ics")
            dialog.save(State.main_window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            Gdk.Display.get_default().get_clipboard().set(self.get_prop("text"))
            State.main_window.add_toast(_("Copied to Clipboard"))  # noqa: F821

        __create_action("edit", __edit)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("export", __export)
        __create_action("move_to_trash", lambda *_: self.delete())

    def __load_sub_tasks(self):
        tasks: list[TaskData] = (
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
            if not t.deleted
        )

        for task in tasks:
            new_task = Task(task, self.task_list, self)
            if task.completed:
                self.completed_tasks_list.append(new_task)
            else:
                self.uncompleted_tasks_list.append(new_task)

        self.expand(self.task_data.expanded)
        self.toggle_visibility(not self.task_data.trash)
        self.update_headerbar()
        self.update_toolbar()
        self.update_tags()
        self.update_color()
        self.update_completion_state()
        self.update_progressbar()

    @property
    def parents_tree(self) -> list[Task]:
        """Get parent tasks chain"""

        parents: list[Task] = []

        def _add(task: Task):
            if isinstance(task.parent, Task):
                parents.append(task.parent)
                _add(task.parent)

        _add(self)

        return parents

    @property
    def tasks(self) -> list[Task]:
        """Top-level Tasks"""

        return self.uncompleted_tasks + self.completed_tasks

    @property
    def all_tasks(self) -> list[Task]:
        """All tasks in the list"""

        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.tasks)

        __add_task(self.tasks)
        return all_tasks

    @property
    def uncompleted_tasks(self) -> list[Task]:
        return get_children(self.uncompleted_tasks_list)

    @property
    def completed_tasks(self) -> list[Task]:
        return get_children(self.completed_tasks_list)

    # ------ PUBLIC METHODS ------ #

    def update_task_data(self) -> None:
        self.task_data = UserData.get_task(self.list_uid, self.uid)

    # --- UPDATE UI FUNCTIONS --- #

    def update_color(self) -> None:
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        if color := self.task_data.color:
            self.main_box.add_css_class(f"task-{color}")

    def update_completion_state(self) -> None:
        completed: bool = self.task_data.completed
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.just_added = True
            self.complete_btn.set_active(completed)
            self.just_added = False

    def update_tasks(self) -> None:
        # Update tasks
        tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
            if not t.deleted
        ]
        tasks_uids: list[str] = [t.uid for t in tasks]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for task in tasks:
            if task.uid not in widgets_uids:
                self.add_task(task)

        for task in self.tasks:
            # Remove task
            if task.uid not in tasks_uids:
                task.purge()
            # Move task to completed tasks
            elif task.get_prop("completed") and task in self.uncompleted_tasks:
                if (
                    len(self.uncompleted_tasks) > 1
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.uncompleted_tasks_list.remove(task)
                self.completed_tasks_list.prepend(task)
            # Move task to uncompleted tasks
            elif not task.get_prop("completed") and task in self.completed_tasks:
                if (
                    len(self.uncompleted_tasks) > 0
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.completed_tasks_list.remove(task)
                self.uncompleted_tasks_list.append(task)

        # Update tasks
        for task in self.tasks:
            task.update_ui()

    def update_ui(self, update_sub_tasks_ui: bool = True) -> None:
        Log.debug(f"Task '{self.uid}: Update UI'")
        self.update_task_data()
        self.toggle_visibility(not self.task_data.trash)
        self.expand(self.task_data.expanded)
        self.update_color()
        self.update_progressbar()
        self.update_completion_state()
        self.update_headerbar()
        self.update_toolbar()
        self.update_tags()
        if update_sub_tasks_ui:
            self.update_tasks()

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_entry_row_applied(self, entry: Adw.EntryRow) -> None:
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.get_prop("text"):
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_ui()
        Sync.sync()

    @Gtk.Template.Callback()
    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.entry_row.props.text = ""
        self.entry_row.emit("apply")

    # --- TOOLBAR --- #
    @Gtk.Template.Callback()
    def _on_menu_toggled(self, _btn: Gtk.MenuButton, active: bool) -> None:
        if not active:
            return

        # Update dates
        created_date: str = datetime.fromisoformat(
            self.get_prop("created_at")
        ).strftime("%Y.%m.%d %H:%M:%S")
        changed_date: str = datetime.fromisoformat(
            self.get_prop("changed_at")
        ).strftime("%Y.%m.%d %H:%M:%S")
        self.created_label.set_label(_("Created:") + " " + created_date)  # noqa: F821
        self.changed_label.set_label(_("Changed:") + " " + changed_date)  # noqa: F821

        # Update color
        color: str = self.get_prop("color")
        for btn in get_children(self.accent_color_btns):
            btn_color = btn.get_buildable_id()
            if btn_color == color:
                self.can_sync = False
                btn.set_active(True)
                self.can_sync = True

    @Gtk.Template.Callback()
    def _on_tags_btn_toggled(self, btn: Gtk.MenuButton, *_) -> None:
        if not btn.get_active():
            return
        tags: list[str] = [t.text for t in UserData.tags]
        tags_list_items: list[TagsListItem] = get_children(self.tags_list)
        tags_list_items_text = [t.title for t in tags_list_items]

        # Remove tags
        for item in tags_list_items:
            if item.title not in tags:
                self.tags_list.remove(item)

        # Add tags
        for tag in tags:
            if tag not in tags_list_items_text:
                self.tags_list.append(TagsListItem(tag, self))

        # Toggle tags
        task_tags: list[str] = [t.title for t in self.tags]
        tags_items: list[TagsListItem] = get_children(self.tags_list)
        for t in tags_items:
            t.block_signals = True
            t.toggle_btn.set_active(t.title in task_tags)
            t.block_signals = False

        self.tags_list.set_visible(len(get_children(self.tags_list)) > 0)

    @Gtk.Template.Callback()
    def _on_accent_color_selected(self, btn: Gtk.CheckButton) -> None:
        if not btn.get_active() or not self.can_sync:
            return
        color: str = btn.get_buildable_id()
        Log.debug(f"Task: change color to '{color}'")
        if color != self.get_prop("color"):
            self.update_props(
                ["color", "synced"], [color if color != "none" else "", False]
            )
            self.update_color()
            Sync.sync()
