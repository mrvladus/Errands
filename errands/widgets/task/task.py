# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from errands.widgets.components.datetime_picker import DateTimePicker
from errands.widgets.task.tag import Tag
from errands.widgets.task.tags_list_item import TagsListItem

if TYPE_CHECKING:
    from errands.widgets.task_list.task_list import TaskList

import os
from datetime import datetime
from icalendar import Calendar, Event

from gi.repository import Adw  # type:ignore
from gi.repository import Gdk  # type:ignore
from gi.repository import Gio  # type:ignore
from gi.repository import GLib  # type:ignore
from gi.repository import GObject  # type:ignore
from gi.repository import Gtk  # type:ignore
from gi.repository import GtkSource  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children, idle_add, timeit


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
    notes_buffer: GtkSource.Buffer = Gtk.Template.Child()
    priority_btn: Gtk.MenuButton = Gtk.Template.Child()
    created_label: Gtk.Label = Gtk.Template.Child()
    changed_label: Gtk.Label = Gtk.Template.Child()
    start_date_time: DateTimePicker = Gtk.Template.Child()
    due_date_time: DateTimePicker = Gtk.Template.Child()
    date_time_btn: Gtk.MenuButton = Gtk.Template.Child()
    date_stack: Adw.ViewStack = Gtk.Template.Child()
    tags_list: Gtk.ListBox = Gtk.Template.Child()
    priority: Gtk.SpinButton = Gtk.Template.Child()
    accent_color_btns: Gtk.Box = Gtk.Template.Child()

    # State
    just_added: bool = True
    can_sync: bool = True
    purged: bool = False
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
        self.window = task_list.window
        self.parent = parent
        GSettings.bind("task-show-progressbar", self.progress_bar_rev, "visible")
        self.__add_actions()
        self.__load_sub_tasks()
        # Set notes theme
        Adw.StyleManager.get_default().bind_property(
            "dark",
            self.notes_buffer,
            "style-scheme",
            GObject.BindingFlags.SYNC_CREATE,
            lambda _, is_dark: self.notes_buffer.set_style_scheme(
                GtkSource.StyleSchemeManager.get_default().get_scheme(
                    "Adwaita-dark" if is_dark else "Adwaita"
                )
            ),
        )
        lm: GtkSource.LanguageManager = GtkSource.LanguageManager.get_default()
        self.notes_buffer.set_language(lm.get_language("markdown"))
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
                except:
                    Log.debug("List: Export cancelled")
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
                self.window.add_toast(_("Exported"))

            dialog = Gtk.FileDialog(initial_name=f"{self.task.uid}.ics")
            dialog.save(self.window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            Gdk.Display.get_default().get_clipboard().set(self.get_prop("text"))
            self.window.add_toast(_("Copied to Clipboard"))

        __create_action("edit", __edit)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("export", __export)
        __create_action("move_to_trash", lambda *_: self.delete())

    @idle_add
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

    # ------ PROPERTIES ------ #

    @property
    def tags(self) -> list[Tag]:
        return [t.get_child() for t in get_children(self.tags_bar)]

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

    def add_task(self, task: TaskData) -> Task:
        Log.info(f"Task List: Add task '{task.uid}'")

        on_top: bool = GSettings.get("task-list-new-task-position-top")
        new_task = Task(task, self, self)
        if on_top:
            self.uncompleted_tasks_list.prepend(new_task)
        else:
            self.uncompleted_tasks_list.append(new_task)

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

    # --- UPDATE UI FUNCTIONS --- #

    def update_color(self):
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        if color := self.task_data.color:
            self.main_box.add_css_class(f"task-{color}")

    def update_completion_state(self):
        completed: bool = self.task_data.completed
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.just_added = True
            self.complete_btn.set_active(completed)
            self.just_added = False

    def update_tags(self):
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

    def update_headerbar(self):
        # Update title
        self.title_row.set_title(Markup.find_url(Markup.escape(self.task_data.text)))

        # Update subtitle
        total, completed = self.get_status()
        self.title_row.set_subtitle(
            _("Completed:") + f" {completed} / {total}" if total > 0 else ""
        )

    def update_progressbar(self):
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

    def update_tasks(self):
        # Update tasks
        data_uids: list[str] = [
            t.uid
            for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
            if not t.deleted
        ]
        widgets_uids: list[str] = [t.uid for t in self.tasks]

        # Add tasks
        for uid in data_uids:
            if uid not in widgets_uids:
                self.add_task(uid)

        for task in self.tasks:
            # Remove task
            if task.uid not in data_uids:
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

    def update_toolbar(self) -> None:
        # Show toolbar
        self.toolbar.set_reveal_child(self.task_data.toolbar_shown)

        # Update Date and Time
        self.due_date_time.datetime = self.task_data.due_date
        self.date_time_btn.get_child().props.label = (
            f"{self.due_date_time.human_datetime}"
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

    def update_ui(self, update_sub_tasks_ui: bool = True) -> None:
        Log.debug(f"Task '{self.uid}: Update UI'")
        if self.purged:
            self.purge()
            return
        self.task_data = UserData.get_task(self.list_uid, self.uid)
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

        self.parent.update_ui()
        self.task_list.update_status()
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

    # --- TOOLBAR --- #

    @Gtk.Template.Callback()
    def _on_date_time_toggled(self, _btn: Gtk.MenuButton, active: bool) -> None:
        self.start_date_time.datetime = self.get_prop("start_date")
        self.due_date_time.datetime = self.get_prop("due_date")

    @Gtk.Template.Callback()
    def _on_date_time_start_set(self, *args) -> None:
        self.update_props(["start_date"], [self.start_date_time.datetime])

    @Gtk.Template.Callback()
    def _on_date_time_due_set(self, *args) -> None:
        self.update_props(["due_date"], [self.due_date_time.datetime])
        self.update_ui()

    @Gtk.Template.Callback()
    def _on_menu_toggled(self, _btn: Gtk.MenuButton, active: bool):
        if not active:
            return

        # Update dates
        created_date: str = datetime.fromisoformat(
            self.get_prop("created_at")
        ).strftime("%Y.%m.%d %H:%M:%S")
        changed_date: str = datetime.fromisoformat(
            self.get_prop("changed_at")
        ).strftime("%Y.%m.%d %H:%M:%S")
        self.created_label.set_label(_("Created:") + " " + created_date)
        self.changed_label.set_label(_("Changed:") + " " + changed_date)

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
    def _on_notes_toggled(self, btn: Gtk.MenuButton, *_):
        notes: str = self.get_prop("notes")
        if btn.get_active():
            self.notes_buffer.set_text(notes)
        else:
            text: str = self.notes_buffer.props.text
            if text == notes:
                return
            Log.info("Task: Change notes")
            self.update_props(["notes", "synced"], [text, False])
            self.update_ui()
            # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_priority_toggled(self, btn: Gtk.MenuButton, *_):
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
    def _on_priority_selected(self, box: Gtk.ListBox, row: Gtk.ListBoxRow):
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
    def _on_accent_color_selected(self, btn: Gtk.CheckButton):
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
