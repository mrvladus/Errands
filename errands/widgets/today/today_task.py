# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from errands.widgets.task.toolbar.toolbar import TaskToolbar

import os
from datetime import datetime

from gi.repository import Adw  # type:ignore
from gi.repository import GLib  # type:ignore
from gi.repository import GObject  # type:ignore
from gi.repository import Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children, timeit
from errands.widgets.task.tag import Tag


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TodayTask(Adw.Bin):
    __gtype_name__ = "TodayTask"

    revealer: Gtk.Revealer = Gtk.Template.Child()
    main_box: Gtk.Box = Gtk.Template.Child()
    title_row: Adw.ActionRow = Gtk.Template.Child()
    complete_btn: Gtk.CheckButton = Gtk.Template.Child()
    entry_row: Adw.EntryRow = Gtk.Template.Child()
    tags_bar: Gtk.FlowBox = Gtk.Template.Child()
    tags_bar_rev: Gtk.Revealer = Gtk.Template.Child()
    # toolbar: TaskToolbar = Gtk.Template.Child()

    # State
    just_added: bool = True
    can_sync: bool = True
    purged: bool = False
    purging: bool = False

    def __init__(self, task: TaskData) -> None:
        super().__init__()

        self.uid = task.uid
        self.list_uid = task.list_uid
        self.just_added = False
        self.title_row.set_title(Markup.find_url(Markup.escape(self.get_prop("text"))))

    def __repr__(self) -> str:
        return f"<class 'Task' {self.uid}>"

    # ------ PRIVATE METHODS ------ #

    # ------ PROPERTIES ------ #

    @property
    def tags(self) -> list[Tag]:
        return [t.get_child() for t in get_children(self.tags_bar)]

    # ------ PUBLIC METHODS ------ #

    def update_tags(self):
        tags: str = self.get_prop("tags")
        tags_list_text: list[str] = [t.title for t in self.tags]

        # Delete tags
        for t in self.tags:
            if t.title not in tags:
                self.tags_bar.remove(t)

        # Add tags
        for t in tags:
            if t not in tags_list_text:
                self.tags_bar.append(Tag(t, self))

        self.tags_bar_rev.set_reveal_child(tags != [])

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

        # Update color
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        if color := self.get_prop("color"):
            self.main_box.add_css_class(f"task-{color}")

        # Update title
        self.title_row.set_title(Markup.find_url(Markup.escape(self.get_prop("text"))))

        # Update crossline and completed toggle
        completed: bool = self.get_prop("completed")
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.just_added = True
            self.complete_btn.set_active(completed)
            self.just_added = False

        # Update toolbar
        self.toolbar.update_ui()

        # Update tags
        self.update_tags()

    # ------ TEMPLATE HANDLERS ------ #

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
