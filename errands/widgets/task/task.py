# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from errands.widgets.task.tag import Tag

if TYPE_CHECKING:
    from errands.widgets.task_list.task_list import TaskList
    from errands.widgets.task.toolbar.toolbar import TaskToolbar

import os
from datetime import datetime

from gi.repository import Adw  # type:ignore
from gi.repository import Gdk  # type:ignore
from gi.repository import Gio  # type:ignore
from gi.repository import GLib  # type:ignore
from gi.repository import GObject  # type:ignore
from gi.repository import Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children, timeit


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Task(Adw.Bin):
    __gtype_name__ = "Task"

    revealer: Gtk.Revealer = Gtk.Template.Child()
    top_drop_area: Gtk.Revealer = Gtk.Template.Child()
    main_box: Gtk.Box = Gtk.Template.Child()
    progress_bar_rev: Gtk.Revealer = Gtk.Template.Child()
    progress_bar: Gtk.ProgressBar = Gtk.Template.Child()
    sub_tasks_revealer: Gtk.Revealer = Gtk.Template.Child()
    sub_tasks: Gtk.Box = Gtk.Template.Child()
    title_row: Adw.ActionRow = Gtk.Template.Child()
    complete_btn: Gtk.CheckButton = Gtk.Template.Child()
    expand_indicator: Gtk.Image = Gtk.Template.Child()
    entry_row: Adw.EntryRow = Gtk.Template.Child()
    tags_bar: Gtk.Box = Gtk.Template.Child()
    toolbar: TaskToolbar = Gtk.Template.Child()

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
    ) -> None:
        super().__init__()

        self.uid = uid
        self.task_list = task_list
        self.list_uid = task_list.list_uid
        self.window = task_list.window
        self.parent = parent
        self.__build_ui()
        self.__add_actions()
        self.just_added = False

    def __repr__(self) -> str:
        return f"<class 'Task' {self.uid}>"

    # ------ PRIVATE METHODS ------ #

    def __add_actions(self) -> None:
        group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="task", group=group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

    def __build_ui(self) -> None:
        GSettings.bind("task-show-progressbar", self.progress_bar_rev, "visible")

        self.title_row.set_title(Markup.find_url(Markup.escape(self.get_prop("text"))))

        # Sub-tasks
        # tasks: list[TaskData] = [
        #     t
        #     for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
        #     if not t.deleted
        # ]
        # for task in tasks:
        #     self.sub_tasks.append(Task(task.uid, self.task_list, self))

    def __sort_tasks(self) -> None:
        def __sort_completed():
            length = len(self.tasks)
            last_idx = length - 1
            i = last_idx
            while i > -1:
                task = self.tasks[i]
                if task.get_prop("completed"):
                    if i != last_idx:
                        UserData.move_task_before(
                            self.list_uid, task.uid, self.tasks[last_idx].uid
                        )
                        self.task_list.reorder_child_after(task, self.tasks[last_idx])
                    last_idx -= 1
                i -= 1

        return
        __sort_completed()

    def __update_tags(self):
        return
        tags: str = self.get_prop("tags")
        if tags != "":
            tags: list[str] = tags.split(",")
        else:
            tags = []

        tags_list: list[Tag] = get_children(self.tags_list)
        tags_list_text: list[str] = [t.title for t in tags_list]

        # Delete tags
        for t in tags_list:
            if t.title not in tags:
                self.tags_list.remove(t)

        # Add tags
        for t in tags:
            if t not in tags_list_text:
                self.add_tag(t)

        self.tags_bar.set_visible(tags != [])

    # ------ PROPERTIES ------ #

    @property
    def tasks(self) -> list[Task]:
        return [t for t in get_children(self.sub_tasks) if isinstance(t, Task)]

    @property
    def all_tasks(self) -> list[Task]:
        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.tasks)

        __add_task(self.tasks)
        return all_tasks

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

    # ------ PUBLIC METHODS ------ #

    def add_tag(self, tag: str):
        Log.debug(f"Task: Add Tag '{tag}'")
        self.tags_bar.append(Tag(tag))

    def add_task(self, uid: str) -> Task:
        on_top: bool = GSettings.get("task-list-new-task-position-top")
        new_task = Task(uid, self.task_list, self)
        if on_top:
            self.sub_tasks.prepend(new_task)
        else:
            self.sub_tasks.append(new_task)
        new_task.update_ui()

        return new_task

    def add_rm_crossline(self, add: bool) -> None:
        if add:
            self.title_row.add_css_class("task-completed")
        else:
            self.title_row.remove_css_class("task-completed")

    def get_prop(self, prop: str) -> Any:
        return UserData.get_prop(self.list_uid, self.uid, prop)

    def get_status(self) -> tuple[int, int]:
        """Get total tasks and completed tasks tuple"""
        return UserData.get_status(self.list_uid, self.uid)

    def delete(
        self,
        *_,
    ) -> None:
        """Move task to trash"""

        Log.info(f"Task: Move to trash: '{self.uid}'")

        self.toggle_visibility(False)
        self.complete_btn.set_active(True)
        self.update_props(["trash", "synced"], [True, False])
        for task in self.all_tasks:
            task.delete(False)
        # if update_task_list_ui:
        #     self.parent.update_ui(False)
        self.window.sidebar.trash_row.update_ui()

    def expand(self, expanded: bool) -> None:
        if expanded != self.get_prop("expanded"):
            self.update_props(["expanded"], [expanded])
        self.sub_tasks_revealer.set_reveal_child(expanded)
        if expanded:
            self.expand_indicator.remove_css_class("expand-indicator-expanded")
        else:
            self.expand_indicator.add_css_class("expand-indicator-expanded")

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
        GLib.idle_add(self.revealer.set_reveal_child, on)

    def update_props(self, props: list[str], values: list[Any]) -> None:
        # Update 'changed_at' if it's not in local props
        local_props: tuple[str] = (
            "deleted",
            "expanded",
            "synced",
            "toolbar_shown",
            "trash",
        )
        for prop in props:
            if prop not in local_props:
                props.append("changed_at")
                values.append(datetime.now().strftime("%Y%m%dT%H%M%S"))
                break
        # Log.debug(f"Task '{self.uid}': Update props {props}")
        UserData.update_props(self.list_uid, self.uid, props, values)

    def update_ui(self, update_sub_tasks_ui: bool = True) -> None:
        Log.debug(f"Task '{self.uid}: Update UI'")
        # Purge
        if self.purged:
            self.purge()
            return

        # Change visibility
        self.toggle_visibility(not self.get_prop("trash"))

        # Expand
        self.expand(self.get_prop("expanded"))

        # Update color
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        if color := self.get_prop("color"):
            self.main_box.add_css_class(f"task-{color}")

        # Update progress bar complete
        if GSettings.get("task-show-progressbar"):
            total, completed = self.get_status()
            pc: int = (
                completed / total * 100
                if total > 0
                else (100 if self.complete_btn.get_active() else 0)
            )
            if self.get_prop("percent_complete") != pc:
                self.update_props(["percent_complete", "synced"], [pc, False])

            self.progress_bar.set_fraction(pc / 100)
            self.progress_bar_rev.set_reveal_child(self.get_status()[0] > 0)

        # Update title
        self.title_row.set_title(Markup.find_url(Markup.escape(self.get_prop("text"))))

        # Update crossline and completed toggle
        completed: bool = self.get_prop("completed")
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.just_added = True
            self.complete_btn.set_active(completed)
            self.just_added = False

        # Update subtitle
        total, completed = self.get_status()
        self.title_row.set_subtitle(
            _("Completed:") + f" {completed} / {total}" if total > 0 else ""
        )

        # Update toolbar
        self.toolbar.update_ui()

        # Update tags
        self.__update_tags()

        data_tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
            if not t.deleted
        ]
        data_uids: list[str] = [t.uid for t in data_tasks]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add sub-tasks
        for task in data_tasks:
            if task.uid not in widgets_uids:
                self.add_task(task.uid)

        # Remove sub-tasks
        for task in self.tasks:
            if task.uid not in data_uids:
                self.sub_tasks.remove(task)

        # Update sub-tasks
        if update_sub_tasks_ui:
            for task in self.tasks:
                task.update_ui()

        # # Sort sub-tasks
        # self.__sort_tasks()

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_title_row_clicked(self, *args):
        self.expand(not self.sub_tasks_revealer.get_child_revealed())

    @Gtk.Template.Callback()
    def _on_sub_task_added(self, entry: Gtk.Entry) -> None:
        text: str = entry.get_text()

        # Return if entry is empty
        if text.strip(" \n\t") == "":
            return

        # Add sub-task
        self.add_task(
            UserData.add_task(
                list_uid=self.list_uid,
                text=text,
                parent=self.uid,
                insert_at_the_top=GSettings.get("task-list-new-task-position-top"),
            )
        )

        # Clear entry
        entry.set_text("")

        # Update status
        if self.get_prop("completed"):
            self.update_props(["completed", "synced"], [False, False])

        self.update_ui(False)

        # Sync
        # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_complete_btn_toggle(self, btn: Gtk.CheckButton) -> None:
        Log.debug(f"Task '{self.uid}': Set completed to '{btn.get_active()}'")

        self.add_rm_crossline(btn.get_active())
        if self.just_added:
            return

        if self.get_prop("completed") != btn.get_active():
            self.update_props(["completed", "synced"], [btn.get_active(), False])

        # Complete all sub-tasks
        if btn.get_active():
            for task in self.all_tasks:
                if not task.get_prop("completed"):
                    task.update_props(["completed", "synced"], [True, False])
                    task.just_added = True
                    task.complete_btn.set_active(True)
                    task.just_added = False

        # Uncomplete parent if sub-task is uncompleted
        else:
            for task in self.parents_tree:
                if task.get_prop("completed"):
                    task.update_props(["completed", "synced"], [False, False])
                    task.just_added = True
                    task.complete_btn.set_active(False)
                    task.just_added = False

        self.task_list.update_ui(False)
        # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_toolbar_btn_toggle(self, btn: Gtk.ToggleButton) -> None:
        if btn.get_active() != self.get_prop("toolbar_shown"):
            self.update_props(["toolbar_shown"], [btn.get_active()])

    @Gtk.Template.Callback()
    def _on_entry_row_applied(self, entry: Adw.EntryRow):
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.get_prop("text"):
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_ui()
        # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.entry_row.props.text = ""
        self.entry_row.emit("apply")

    # --- DND --- #

    @Gtk.Template.Callback()
    def _on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        # Bug workaround when task is not sensitive after short dnd
        for task in self.task_list.all_tasks:
            task.set_sensitive(True)
        self.set_sensitive(False)
        value: GObject.Value = GObject.Value(Task)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    @Gtk.Template.Callback()
    def _on_drag_begin(self, _, drag) -> bool:
        text: str = self.get_prop("text")
        icon: Gtk.DragIcon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(Gtk.Button(label=text if len(text) < 20 else f"{text[0:20]}..."))

    @Gtk.Template.Callback()
    def _on_drag_end(self, *_) -> bool:
        self.set_sensitive(True)
        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

    @Gtk.Template.Callback()
    def _on_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """

        if task.parent == self:
            return

        # Change list
        if task.list_uid != self.list_uid:
            UserData.move_task_to_list(
                task.uid,
                task.list_uid,
                self.list_uid,
                self.get_prop("uid"),
                False,
            )

        # Change parent
        task.update_props(["parent", "synced"], [self.uid, False])

        # Toggle completion
        if not task.get_prop("completed") and self.get_prop("completed"):
            self.update_props(["completed", "synced"], [False, False])
            for parent in self.parents_tree:
                if parent.get_prop("completed"):
                    parent.update_props(["completed", "synced"], [False, False])

        # Expand sub-tasks
        if not self.get_prop("expanded"):
            self.expand(True)

        # Remove from old position
        task.parent.task_list_model.remove(task.get_index())

        # Update UI
        self.task_list.update_ui()
        if task.task_list != self.task_list:
            task.task_list.update_ui()

        # Sync
        # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_top_area_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on "+" area on top of task
        """

        if task.list_uid != self.list_uid:
            UserData.move_task_to_list(
                task.uid,
                task.list_uid,
                self.list_uid,
                self.parent.uid if isinstance(self.parent, Task) else "",
                False,
            )
        UserData.move_task_before(self.list_uid, task.uid, self.uid)

        # If task has the same parent
        if task.parent == self.parent:
            # Insert into new position
            self.parent.task_list_model.insert(
                self.get_index(), Task(task.uid, self.task_list, self.parent)
            )
            # Remove from old position
            self.parent.task_list_model.remove(task.get_index())
        # Change parent if different parents
        else:
            UserData.update_props(
                self.list_uid,
                task.uid,
                ["parent", "synced"],
                [self.parent.uid if isinstance(self.parent, Task) else "", False],
            )

            # Toggle completion for parents
            if not task.get_prop("completed"):
                for parent in self.parents_tree:
                    if parent.get_prop("completed"):
                        parent.update_props(["completed", "synced"], [False, False])

            # Insert into new position
            self.parent.task_list_model.insert(
                self.parent.task_list_model.find(self)[1],
                Task(task.uid, self.task_list, self.parent),
            )

            # Remove from old position
            task.parent.task_list_model.remove(
                task.parent.task_list_model.find(task)[1]
            )

        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

        # Update UI
        self.task_list.update_ui()

        # Sync
        # Sync.sync(False)
