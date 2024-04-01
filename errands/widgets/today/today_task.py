# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from errands.widgets.task.task import Task
    from errands.widgets.window import Window

import os
from datetime import datetime

from gi.repository import (  # type:ignore
    Adw,  # type:ignore
    Gdk,  # type:ignore
    Gio,  # type:ignore
    GLib,  # type:ignore
    Gtk,  # type:ignore
)
from icalendar import Calendar, Event

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.markup import Markup

# from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.widgets.components.datetime_window import DateTimeWindow
from errands.widgets.components.notes_window import NotesWindow
from errands.widgets.task.tag import Tag
from errands.widgets.task.tags_list_item import TagsListItem


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
    notes_btn: Gtk.MenuButton = Gtk.Template.Child()
    priority_btn: Gtk.MenuButton = Gtk.Template.Child()
    created_label: Gtk.Label = Gtk.Template.Child()
    changed_label: Gtk.Label = Gtk.Template.Child()
    date_time_btn: Gtk.MenuButton = Gtk.Template.Child()
    tags_list: Gtk.ListBox = Gtk.Template.Child()
    priority: Gtk.SpinButton = Gtk.Template.Child()
    accent_color_btns: Gtk.Box = Gtk.Template.Child()

    linked_task: Task = None

    # State
    just_added: bool = True
    can_sync: bool = True
    purged: bool = False
    purging: bool = False

    def __init__(self, task_data: TaskData) -> None:
        super().__init__()
        self.task_data = task_data
        self.uid = task_data.uid
        self.list_uid = task_data.list_uid
        self.window: Window = Adw.Application.get_default().get_active_window()
        self.notes_window: NotesWindow = NotesWindow(self)
        self.datetime_window: DateTimeWindow = DateTimeWindow(self)
        self.__add_actions()
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
                self.window.add_toast(_("Exported"))  # noqa: F821

            dialog = Gtk.FileDialog(initial_name=f"{self.task_data.uid}.ics")
            dialog.save(self.window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            Gdk.Display.get_default().get_clipboard().set(self.get_prop("text"))
            self.window.add_toast(_("Copied to Clipboard"))  # noqa: F821

        __create_action("edit", __edit)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("export", __export)
        __create_action("move_to_trash", lambda *_: self.delete())

    def __find_linked_task(self) -> None:
        for list in self.window.sidebar.task_lists:
            if list.list_uid == self.list_uid:
                for task in list.all_tasks:
                    if task.uid == self.uid:
                        self.linked_task = task
                        return

    def __update_linked_task_ui(self):
        if not self.linked_task:
            self.__find_linked_task()
        self.linked_task.update_ui()

    # ------ PROPERTIES ------ #

    @property
    def tags(self) -> list[Tag]:
        return [t.get_child() for t in get_children(self.tags_bar)]

    # ------ PUBLIC METHODS ------ #

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

    def delete(self, *_) -> None:
        pass

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
        UserData.update_props(self.list_uid, self.uid, props, values)
        # Purge self if date set other than today
        if "due_date" in props:
            if (
                datetime.fromisoformat(values[props.index("due_date")]).date()
                != datetime.today().date()
            ):
                self.purge()
        # Update linked task every time TodayTask is changes props
        self.__update_linked_task_ui()

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

    def update_tags(self) -> None:
        tags: str = self.task_data.tags
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

    def update_headerbar(self) -> None:
        # Update title
        self.title_row.set_title(Markup.find_url(Markup.escape(self.task_data.text)))

    def update_toolbar(self) -> None:
        # Update Date and Time
        if self.datetime_window:
            self.datetime_window.due_date_time.datetime = self.task_data.due_date
            self.date_time_btn.get_child().props.label = (
                f"{self.datetime_window.due_date_time.human_datetime}"
            )

        # Update notes button css
        if self.task_data.notes:
            self.notes_btn.add_css_class("accent")
        else:
            self.notes_btn.remove_css_class("accent")

        # Update priority button css
        priority: int = self.task_data.priority
        self.priority_btn.props.css_classes = ["flat"]
        if 0 < priority < 5:
            self.priority_btn.add_css_class("error")
        elif 4 < priority < 9:
            self.priority_btn.add_css_class("warning")
        elif priority == 9:
            self.priority_btn.add_css_class("accent")
        self.priority_btn.set_icon_name(
            f"errands-priority{'-set' if priority>0 else ''}-symbolic"
        )

    def update_ui(self) -> None:
        Log.debug(f"Task '{self.uid}: Update UI'")
        if self.purged:
            self.purge()
            return
        self.task_data = UserData.get_task(self.list_uid, self.uid)
        self.toggle_visibility(not self.task_data.trash)
        self.update_color()
        self.update_completion_state()
        self.update_headerbar()
        self.update_toolbar()
        self.update_tags()

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_complete_btn_toggle(self, btn: Gtk.CheckButton) -> None:
        pass

    @Gtk.Template.Callback()
    def _on_entry_row_applied(self, entry: Adw.EntryRow) -> None:
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

    # --- TOOLBAR --- #

    @Gtk.Template.Callback()
    def _on_date_time_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.datetime_window.show()

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
    def _on_notes_btn_clicked(self, btn: Gtk.Button) -> None:
        self.notes_window.show()

    @Gtk.Template.Callback()
    def _on_priority_toggled(self, btn: Gtk.MenuButton, *_) -> None:
        priority: int = self.get_prop("priority")
        if btn.get_active():
            self.priority.set_value(priority)
        else:
            new_priority: int = self.priority.get_value_as_int()
            if priority != new_priority:
                Log.debug(f"Task Toolbar: Set priority to '{new_priority}'")
                self.update_props(["priority", "synced"], [new_priority, False])
                self.update_ui()
                # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_priority_selected(self, box: Gtk.ListBox, row: Gtk.ListBoxRow) -> None:
        rows: list[Gtk.ListBoxRow] = get_children(box)
        for i, r in enumerate(rows):
            if r == row:
                index = i
                break
        match index:
            case 0:
                self.priority.set_value(1)
            case 1:
                self.priority.set_value(5)
            case 2:
                self.priority.set_value(9)
            case 3:
                self.priority.set_value(0)
        self.priority_btn.popdown()

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
            # Sync.sync(False)
