# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from datetime import datetime
import os
from typing import Any, TYPE_CHECKING
from icalendar import Calendar, Event

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList

from gi.repository import Gtk, Adw, Gdk, GObject, GLib, GtkSource, Gio  # type:ignore
from errands.lib.sync.sync import Sync
from errands.lib.logging import Log
from errands.lib.data import TaskData, UserData
from errands.lib.markup import Markup
from errands.lib.utils import get_children
from errands.lib.gsettings import GSettings


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


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class Task(Gtk.Revealer):
    __gtype_name__ = "Task"

    GObject.type_register(GtkSource.View)
    GObject.type_register(GtkSource.Buffer)

    top_drop_area: Gtk.Revealer = Gtk.Template.Child()
    main_box: Gtk.Box = Gtk.Template.Child()
    progress_bar_rev: Gtk.Revealer = Gtk.Template.Child()
    progress_bar: Gtk.ProgressBar = Gtk.Template.Child()
    sub_tasks_revealer: Gtk.Revealer = Gtk.Template.Child()
    sub_tasks: Gtk.ListBox = Gtk.Template.Child()
    title_row: Adw.ActionRow = Gtk.Template.Child()
    complete_btn: Gtk.CheckButton = Gtk.Template.Child()
    expand_indicator: Gtk.Image = Gtk.Template.Child()
    entry_row: Adw.EntryRow = Gtk.Template.Child()
    toolbar_rev: Gtk.Revealer = Gtk.Template.Child()
    notes_btn: Gtk.MenuButton = Gtk.Template.Child()
    notes_buffer: GtkSource.Buffer = Gtk.Template.Child()
    priority_btn: Gtk.MenuButton = Gtk.Template.Child()
    created_label: Gtk.Label = Gtk.Template.Child()
    changed_label: Gtk.Label = Gtk.Template.Child()

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
        self.__build_ui()
        self.__add_actions()
        self.just_added = False

    def __add_actions(self) -> None:
        group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="task", group=group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

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
                    if i["uid"] == self.uid
                ][0]
                calendar = Calendar()
                event = Event()
                event.add("uid", task["uid"])
                event.add("summary", task["text"])
                if task["notes"]:
                    event.add("description", task["notes"])
                event.add("priority", task["priority"])
                if task["tags"]:
                    event.add("categories", task["tags"])
                event.add("percent-complete", task["percent_complete"])
                if task["color"]:
                    event.add("x-errands-color", task["color"])
                event.add(
                    "dtstart",
                    (
                        datetime.fromisoformat(task["start_date"])
                        if task["start_date"]
                        else datetime.now()
                    ),
                )
                if task["end_date"]:
                    event.add("dtend", datetime.fromisoformat(task["end_date"]))
                calendar.add_component(event)

                with open(file.get_path(), "wb") as f:
                    f.write(calendar.to_ical())
                self.window.add_toast(_("Exported"))

            dialog = Gtk.FileDialog(initial_name=f"{self.uid}.ics")
            dialog.save(self.window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            Gdk.Display.get_default().get_clipboard().set(self.get_prop("text"))
            self.window.add_toast(_("Copied to Clipboard"))

        __create_action("edit", __edit)
        __create_action("export", __export)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("move_to_trash", lambda *_: self.delete())

    def __build_ui(self) -> None:
        GSettings.bind("task-show-progressbar", self.progress_bar_rev, "visible")
        self.set_reveal_child(True)

        self.title_row.set_title(Markup.find_url(Markup.escape(self.get_prop("text"))))

        # Toolbar
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

        # Sub-tasks
        self.__load_sub_tasks()

        # def sort(row1: Gtk.ListBoxRow, row2: Gtk.ListBoxRow) -> int:
        #     a1 = int(row1.get_child().task_row.complete_btn.get_active())
        #     a2 = int(row2.get_child().task_row.complete_btn.get_active())
        #     print(row1, a1, row2, a2)
        #     if a1 > a2:
        #         return 1
        #     elif a1 < a2:
        #         return -1
        #     else:
        #         return 0

        # def header(row: Gtk.ListBoxRow, before: Gtk.ListBoxRow):
        #     if not before:
        #         return
        #     if (
        #         row.get_child().task_row.complete_btn.get_active()
        #         and not before.get_child().task_row.complete_btn.get_active()
        #     ):
        #         row.set_header(
        #             Gtk.Separator(
        #                 margin_bottom=3, margin_top=3, margin_end=12, margin_start=12
        #             )
        #         )
        #     else:
        #         row.set_header(None)

        # self.sub_tasks.set_sort_func(sort)
        # self.sub_tasks.set_header_func(header)

    def __load_sub_tasks(self):
        tasks: list[TaskData] = [
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
            if not t["deleted"]
        ]
        for task in tasks:
            self.sub_tasks.append(
                Gtk.ListBoxRow(
                    child=Task(task["uid"], self.task_list, self, False),
                    activatable=False,
                    css_classes=["task"],
                )
            )

        # for task in tasks:
        #     if task["completed"]:
        #         completed_tasks_model.append(
        #             Task(task["uid"], self.task_list, self, True)
        #         )
        #     else:
        #         uncompleted_tasks_model.append(
        #             Task(task["uid"], self.task_list, self, True)
        #         )

    def add_rm_crossline(self, add: bool) -> None:
        if add:
            self.title_row.add_css_class("task-completed")
        else:
            self.title_row.remove_css_class("task-completed")

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
        if prop in "deleted completed expanded trash toolbar_shown":
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
        self.complete_btn.set_active(True)
        self.update_props(["trash", "synced"], [True, False])
        for task in self.all_tasks:
            task.delete()
        self.parent.update_ui()
        self.window.sidebar.trash_item.update_ui()

    def expand(self, expanded: bool) -> None:
        self.sub_tasks_revealer.set_reveal_child(expanded)
        self.update_props(["expanded"], [expanded])
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

        # Show toolbar
        self.toolbar_rev.set_reveal_child(self.get_prop("toolbar_shown"))

        # Update notes button css
        if self.get_prop("notes"):
            self.notes_btn.add_css_class("accent")
        else:
            self.notes_btn.remove_css_class("accent")

        # Update priority button css
        priority: int = self.get_prop("priority")
        self.priority_btn.props.css_classes = ["flat"]
        if 0 < priority < 5:
            self.priority_btn.add_css_class("error")
        elif 4 < priority < 9:
            self.priority_btn.add_css_class("warning")
        elif priority == 9:
            self.priority_btn.add_css_class("accent")

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_title_row_clicked(self, *args):
        self.expand(not self.sub_tasks_revealer.get_child_revealed())

    @Gtk.Template.Callback()
    def _on_top_area_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on "+" area on top of task
        """

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
            # Move widget
            self.get_parent().reorder_child_after(task, self)
            self.get_parent().reorder_child_after(self, task)
            return True
        # Change parent if different parents
        UserData.update_props(
            self.list_uid,
            task.uid,
            ["parent", "synced"],
            [self.parent.uid if isinstance(self.parent, Task) else "", False],
        )
        # Add new task widget
        new_task: Task = Task(
            task.uid,
            self.task_list,
            self.parent,
            self.get_prop("parent") != None,
        )
        self.get_parent().append(new_task)
        self.get_parent().reorder_child_after(new_task, self)
        self.get_parent().reorder_child_after(self, new_task)
        new_task.toggle_visibility(True)
        # Toggle completion
        if not task.complete_btn.get_active():
            self.update_props(["completed", "synced"], [False, False])
            self.just_added = True
            self.complete_btn.set_active(False)
            self.just_added = False
            for parent in self.get_parents_tree():
                if parent.complete_btn.get_active():
                    parent.update_props(["completed", "synced"], [False, False])
                    parent.just_added = True
                    parent.complete_btn.set_active(False)
                    parent.just_added = False
        # Update status
        task.purge()
        self.task_list.update_ui()
        # Sync
        Sync.sync()

    @Gtk.Template.Callback()
    def _on_sub_task_added(self, entry: Gtk.Entry) -> None:
        text: str = entry.get_text()

        # Return if entry is empty
        if text.strip(" \n\t") == "":
            return

        # Add sub-task
        UserData.add_task(
            list_uid=self.list_uid,
            text=text,
            parent=self.uid,
            insert_at_the_top=GSettings.get("task-list-new-task-position-top"),
        )

        # Clear entry
        entry.set_text("")

        # Update status
        self.update_props(["completed", "synced"], [False, False])
        self.just_added = True
        self.complete_btn.set_active(False)
        self.just_added = False
        self.update_ui()

        # Sync
        Sync.sync()

    @Gtk.Template.Callback()
    def _on_complete_btn_toggle(self, btn: Gtk.CheckButton) -> None:
        self.get_parent().changed()
        return
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

    @Gtk.Template.Callback()
    def _on_toolbar_btn_toggle(self, btn: Gtk.ToggleButton) -> None:
        self.update_props(["toolbar_shown"], [btn.get_active()])

    @Gtk.Template.Callback()
    def _on_entry_row_applied(self, entry: Adw.EntryRow):
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.get_prop("text"):
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_ui()
        Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.entry_row.props.text = ""
        self.entry_row.emit("apply")

    @Gtk.Template.Callback()
    def _on_notes_toggled(self, btn: Gtk.MenuButton, *_):
        notes: str = self.get_prop("notes")
        if btn.get_active():
            self.notes_buffer.set_text(notes)
            self.update_ui()
        else:
            text: str = self.notes_buffer.props.text
            if text == notes:
                return
            Log.info("Task: Change notes")
            self.update_props(["notes", "synced"], [text, False])
            self.update_ui()
            Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_priority_toggled(self, btn: Gtk.MenuButton, *_):
        if not btn.get_active():
            return
        # priority: int = self.get_prop("priority")
        # for btn in get_children(priority_box):
        #     if btn.priority == 0 == priority:
        #         btn.set_active(True)
        #     elif btn.priority == 1 and 0 < priority < 5:
        #         btn.set_active(True)
        #     elif btn.priority == 5 and 4 < priority < 9:
        #         btn.set_active(True)
        #     elif btn.priority == 9 and 8 < priority:
        #         btn.set_active(True)

    # @Gtk.Template.Callback()
    # def _on_priority_selected(self, btn: Gtk.ToggleButton, priority: int):
    #     box: Gtk.Box = btn.get_parent()
    #     for button in get_children(box):
    #         if button != btn:
    #             button.set_active(False)
    #     self.update_props(["priority", "synced"], [priority, False])
    #     self.update_ui()
    #     Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_accent_color_toggled(self, _, btn: Gtk.MenuButton):
        return
        self.can_sync = False
        color: str = self.get_prop("color")
        if color:
            for btn in get_children(color_box):
                for css_class in btn.get_css_classes():
                    if color in css_class:
                        btn.set_active(True)
        else:
            color_box.get_first_child().set_active(True)
        self.can_sync = True

    def __on_accent_color_selected(self, btn: Gtk.CheckButton, color: str):
        if not btn.get_active() or not self.can_sync:
            return

        Log.info(f"Task: change color to '{color}'")
        self.update_props(
            ["color", "synced"], [color if color != "none" else "", False]
        )
        self.update_ui()
        Sync.sync(False)

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

        if task == task or task.parent == self:
            return

        # Change parent
        UserData.move_task_to_list(
            task.uid,
            task.list_uid,
            self.list_uid,
            self.get_prop("uid"),
            False,
        )
        # Toggle completion
        if not task.complete_btn.get_active():
            self.update_props(["completed", "synced"], [False, False])
            # self.task.just_added = True
            # self.task.task_row.complete_btn.set_active(False)
            # self.task.just_added = False
            for parent in self.get_parents_tree():
                if parent.complete_btn.get_active():
                    parent.update_props(["completed", "synced"], [False, False])
                    # parent.just_added = True
                    # parent.task_row.complete_btn.set_active(False)
                    # parent.just_added = False
        if not self.get_prop("expanded"):
            self.expand(True)
        # Remove old task
        task.purged = True
        self.task_list.update_ui()
        # Sync
        Sync.sync()

        return True
