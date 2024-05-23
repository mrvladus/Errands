# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from gi.repository import Adw, Gdk, Gio, GLib, GObject, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import (
    ErrandsButton,
    ErrandsCheckButton,
    ErrandsToggleButton,
)
from errands.widgets.shared.components.entries import ErrandsEntry
from errands.widgets.shared.components.menus import ErrandsMenuItem, ErrandsSimpleMenu
from errands.widgets.shared.task_toolbar import ErrandsTaskToolbar
from errands.widgets.shared.titled_separator import TitledSeparator

if TYPE_CHECKING:
    from errands.widgets.task_list.task_list import TaskList


class Task(Gtk.Revealer):
    block_signals: bool = True
    purging: bool = False
    can_sync: bool = True

    def __init__(self, task_data: TaskData, parent: TaskList | Task) -> None:
        super().__init__()
        self.task_data = task_data
        self.list_uid = task_data.list_uid
        self.uid = task_data.uid
        self.parent = parent
        self.__build_ui()
        self.__add_actions()
        self.__load_sub_tasks()
        self.block_signals = False

    def __repr__(self) -> str:
        return f"<class 'Task' {self.task_data.uid}>"

    def __add_actions(self) -> None:
        self.group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="task", group=self.group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction(name=name)
            action.connect("activate", callback)
            self.group.add_action(action)

        def __edit(*args):
            self.edit_row.set_text(self.task_data.text)
            self.edit_row.set_visible(True)
            self.edit_row.grab_focus()

        def __export(*args):
            def __confirm(dialog, res):
                try:
                    file = dialog.save_finish(res)
                except Exception as e:
                    Log.debug(f"List: Export cancelled. {e}")
                    return

                Log.info(f"Task: Export '{self.uid}'")

                with open(file.get_path(), "w") as f:
                    f.write(self.task_data.to_ical(True))

                State.main_window.add_toast(_("Exported"))

            dialog = Gtk.FileDialog(initial_name=f"{self.uid}.ics")
            dialog.save(State.main_window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            self.get_clipboard().set(self.task_data.text)
            State.main_window.add_toast(_("Copied to Clipboard"))

        __create_action("edit", __edit)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("export", __export)
        __create_action("move_to_trash", lambda *_: self.delete())

    def __build_ui(self):
        # --- TOP DROP AREA --- #
        top_drop_area_img: Gtk.Image = Gtk.Image(
            icon_name="errands-add-symbolic",
            margin_start=12,
            margin_end=12,
            css_classes=["task-drop-area"],
        )
        top_drop_area_drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
            type=Task, actions=Gdk.DragAction.MOVE
        )
        top_drop_area_drop_ctrl.connect("drop", self._on_task_top_area_drop)
        top_drop_area_img.add_controller(top_drop_area_drop_ctrl)
        self.top_drop_area = Gtk.Revealer(child=top_drop_area_img)

        # Drop Motion Controller
        drop_controller: Gtk.DropControllerMotion = Gtk.DropControllerMotion()
        drop_controller.bind_property(
            "contains-pointer",
            self.top_drop_area,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE,
        )
        self.add_controller(drop_controller)

        # --- TITLE --- #

        title_box: Gtk.ListBox = Gtk.ListBox(
            css_classes=["transparent", "rounded-corners"]
        )

        # Hover controller
        hover_ctrl: Gtk.EventControllerMotion = Gtk.EventControllerMotion()
        title_box.add_controller(hover_ctrl)

        # Click controller
        click_ctrl: Gtk.GestureClick = Gtk.GestureClick()
        click_ctrl.connect("released", self._on_title_row_clicked)
        title_box.add_controller(click_ctrl)

        # Drop controller
        drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
            type=Task, actions=Gdk.DragAction.MOVE
        )
        drop_ctrl.connect("drop", self._on_task_drop)
        title_box.add_controller(drop_ctrl)

        # Drag controller
        drag_ctrl: Gtk.DragSource = Gtk.DragSource(actions=Gdk.DragAction.MOVE)
        drag_ctrl.connect("prepare", self._on_drag_prepare)
        drag_ctrl.connect("drag-begin", self._on_drag_begin)
        drag_ctrl.connect("drag-cancel", self._on_drag_end)
        drag_ctrl.connect("drag-end", self._on_drag_end)
        title_box.add_controller(drag_ctrl)

        # Title row
        self.title_row = Adw.ActionRow(
            height_request=60,
            use_markup=True,
            tooltip_text=_("Toggle Sub-Tasks"),
            cursor=Gdk.Cursor(name="pointer"),
            css_classes=["transparent", "rounded-corners"],
        )
        title_box.append(self.title_row)

        # Complete button
        self.complete_btn: ErrandsCheckButton = ErrandsCheckButton(
            tooltip_text=_("Toggle Completion"),
            valign=Gtk.Align.CENTER,
            css_classes=["selection-mode"],
            on_toggle=self._on_complete_btn_toggled,
        )
        self.title_row.add_prefix(self.complete_btn)

        # Expand indicator
        self.expand_indicator = Gtk.Image(
            icon_name="errands-up-symbolic",
            css_classes=["expand-indicator"],
            halign=Gtk.Align.END,
        )
        expand_indicator_rev = Gtk.Revealer(
            child=self.expand_indicator,
            transition_type=1,
            reveal_child=False,
            can_target=False,
        )
        hover_ctrl.bind_property(
            "contains-pointer",
            expand_indicator_rev,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE,
        )

        # Toolbar toggle
        self.toolbar_toggle_btn: ErrandsToggleButton = ErrandsToggleButton(
            icon_name="errands-toolbar-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Toggle Toolbar"),
            css_classes=["circular", "flat"],
            on_toggle=self._on_toolbar_toggle_btn_toggled,
        )

        # Right click menu
        right_click_ctrl = Gtk.GestureClick(button=3)
        right_click_ctrl.connect("released", self.__on_right_click)
        self.title_row.add_controller(right_click_ctrl)

        self.popover_menu: Gtk.PopoverMenu = Gtk.PopoverMenu(
            halign=Gtk.Align.START,
            has_arrow=False,
            menu_model=ErrandsSimpleMenu(
                items=(
                    ErrandsMenuItem(_("Edit"), "task.edit"),
                    ErrandsMenuItem(_("Move to Trash"), "task.move_to_trash"),
                    ErrandsMenuItem(_("Copy to Clipboard"), "task.copy_to_clipboard"),
                    ErrandsMenuItem(_("Export"), "task.export"),
                )
            ),
        )

        self.title_row.add_suffix(
            ErrandsBox(
                spacing=6,
                children=[
                    expand_indicator_rev,
                    self.toolbar_toggle_btn,
                ],
            )
        )

        # Edit row
        self.edit_row: Adw.EntryRow = Adw.EntryRow(
            title=_("Edit"),
            visible=False,
            height_request=60,
            show_apply_button=True,
            css_classes=["transparent", "rounded-corners"],
        )
        self.edit_row.bind_property(
            "visible",
            self.title_row,
            "visible",
            GObject.BindingFlags.SYNC_CREATE
            | GObject.BindingFlags.INVERT_BOOLEAN
            | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        self.edit_row.connect("apply", self._on_edit_row_applied)
        title_box.append(self.edit_row)

        # Cancel edit button
        self.edit_row.add_suffix(
            ErrandsButton(
                tooltip_text=_("Cancel"),
                valign=Gtk.Align.CENTER,
                icon_name="window-close-symbolic",
                css_classes=["circular"],
                on_click=self._on_cancel_edit_btn_clicked,
            )
        )

        # --- TAGS BAR --- #

        self.tags_bar: Gtk.FlowBox = Gtk.FlowBox(
            height_request=20,
            margin_start=12,
            margin_end=12,
            margin_bottom=3,
            selection_mode=Gtk.SelectionMode.NONE,
            max_children_per_line=1000,
        )
        self.tags_bar_rev: Gtk.Revealer = Gtk.Revealer(child=self.tags_bar)

        # --- PROGRESS BAR --- #

        self.progress_bar: Gtk.ProgressBar = Gtk.ProgressBar(
            margin_start=12,
            margin_end=12,
            margin_bottom=2,
        )
        self.progress_bar.add_css_class("osd")
        self.progress_bar.add_css_class("dim-label")
        self.progress_bar_rev: Gtk.Revealer = Gtk.Revealer(
            child=self.progress_bar, reveal_child=True
        )

        # --- TOOL BAR --- #

        self.toolbar_rev: Gtk.Revealer = Gtk.Revealer()
        self.toolbar_toggle_btn.bind_property(
            "active",
            self.toolbar_rev,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        # Build toolbar if needed
        self.toolbar = None
        if self.task_data.toolbar_shown:
            self.__build_toolbar()

        # --- SUB TASKS --- #

        # Uncompleted tasks
        self.uncompleted_task_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
        )

        # Separator
        self.task_lists_separator: Adw.Bin = Adw.Bin(
            child=TitledSeparator(_("Completed"), (24, 24, 0, 0))
        )

        # Completed tasks
        self.completed_task_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
        )
        self.completed_task_list.bind_property(
            "visible",
            self.task_lists_separator,
            "visible",
            GObject.BindingFlags.SYNC_CREATE,
        )

        self.sub_tasks = Gtk.Revealer(
            child=ErrandsBox(
                orientation=Gtk.Orientation.VERTICAL,
                css_classes=["sub-tasks"],
                children=[
                    ErrandsEntry(
                        margin_start=12,
                        margin_end=12,
                        margin_top=3,
                        margin_bottom=4,
                        placeholder_text=_("Add new Sub-Task"),
                        on_activate=self._on_sub_task_added,
                    ),
                    self.uncompleted_task_list,
                    self.task_lists_separator,
                    self.completed_task_list,
                ],
            )
        )

        # --- MAIN BOX --- #

        self.main_box: ErrandsBox = ErrandsBox(
            orientation=Gtk.Orientation.VERTICAL,
            margin_start=12,
            margin_end=12,
            css_classes=["card", "fade"],
            children=[
                title_box,
                self.popover_menu,
                self.tags_bar_rev,
                self.progress_bar_rev,
                self.toolbar_rev,
                self.sub_tasks,
            ],
        )

        self.set_child(
            ErrandsBox(
                orientation=Gtk.Orientation.VERTICAL,
                margin_bottom=6,
                margin_top=6,
                children=[self.top_drop_area, self.main_box],
            )
        )

    def __build_toolbar(self) -> None:
        """
        Separate func for building toolbar.
        Needed for lazy loading of the toolbar.
        If toolbar doesn't need to be shown on startup - don't load it.
        It's saves memory. About 50 Mb for each 100 Tasks.
        """

        self.toolbar = ErrandsTaskToolbar(self)
        self.toolbar_rev.set_child(self.toolbar)

    def __load_sub_tasks(self) -> None:
        tasks: list[TaskData] = (
            t
            for t in UserData.tasks
            if not t.deleted and t.list_uid == self.list_uid and t.parent == self.uid
        )
        for task in tasks:
            new_task = Task(task, self)
            if task.completed:
                self.completed_task_list.append(new_task)
            else:
                self.uncompleted_task_list.append(new_task)

        self.set_reveal_child(self.task_data.expanded)
        self.toggle_visibility(not self.task_data.trash)
        self.expand(self.task_data.expanded)
        self.update_color()
        self.update_title()
        self.update_progress_bar()
        self.update_tags_bar()
        self.update_toolbar()

    # ------ PROPERTIES ------ #

    @property
    def task_list(self) -> TaskList:
        return State.get_task_list(self.task_data.list_uid)

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
    def tags(self) -> list[Tag]:
        return [item.get_child() for item in get_children(self.tags_bar)]

    @property
    def all_tasks(self) -> list[Task]:
        """All sub tasks"""

        all_tasks: list[Task] = []

        def __add_task(tasks: list[Task]) -> None:
            for task in tasks:
                all_tasks.append(task)
                __add_task(task.tasks)

        __add_task(self.tasks)
        return all_tasks

    @property
    def tasks(self) -> list[Task]:
        """Top-level Tasks"""

        return self.uncompleted_tasks + self.completed_tasks

    @property
    def uncompleted_tasks(self) -> list[Task]:
        return get_children(self.uncompleted_task_list)

    @property
    def completed_tasks(self) -> list[Task]:
        return get_children(self.completed_task_list)

    # ------ PUBLIC METHODS ------ #

    def add_tag(self, tag: str) -> None:
        self.tags_bar.append(Tag(tag, self))

    def add_rm_crossline(self, add: bool) -> None:
        if add:
            self.title_row.add_css_class("task-completed")
        else:
            self.title_row.remove_css_class("task-completed")

    def add_task(self, task: TaskData) -> Task:
        Log.info(f"Task '{self.uid}': Add sub-task '{task.uid}'")

        new_task: Task = Task(task, self)
        if task.completed:
            self.completed_task_list.prepend(new_task)
        else:
            if GSettings.get("task-list-new-task-position-top"):
                self.uncompleted_task_list.prepend(new_task)
            else:
                self.uncompleted_task_list.append(new_task)

    def get_prop(self, prop: str) -> Any:
        return UserData.get_prop(self.list_uid, self.uid, prop)

    def get_status(self) -> tuple[int, int]:
        """Get total tasks and completed tasks tuple"""
        return UserData.get_status(self.list_uid, self.uid)

    def delete(self, *_) -> None:
        """Move task to trash"""

        Log.info(f"Task: Move to trash: '{self.uid}'")

        self.toggle_visibility(False)
        self.update_props(["trash"], [True])
        self.complete_btn.set_active(True)
        State.tasks_page.update_ui()
        State.trash_sidebar_row.update_ui()
        State.tags_page.update_ui()
        self.task_list.update_title()
        if isinstance(self.parent, Task):
            self.parent.update_title()
            self.parent.update_progress_bar()

    def expand(self, expanded: bool) -> None:
        if expanded != self.task_data.expanded:
            self.update_props(["expanded"], [expanded])
        self.sub_tasks.set_reveal_child(expanded)
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
        GLib.timeout_add(300, __finish_remove)

    def update_props(self, props: list[str], values: list[Any]) -> None:
        # Update 'changed_at' if it's not in local props
        local_props: tuple[str] = (
            "deleted",
            "expanded",
            "synced",
            "toolbar_shown",
            "trash",
            "color",
            "notified",
        )
        for prop in props:
            if prop not in local_props:
                props.append("changed_at")
                values.append(datetime.now().strftime("%Y%m%dT%H%M%S"))
                break
        if "synced" not in props and "changed_at" in props:
            props.append("synced")
            values.append(False)
        UserData.update_props(self.list_uid, self.uid, props, values)

    def toggle_visibility(self, on: bool) -> None:
        GLib.idle_add(self.set_reveal_child, on)

    def update_title(self) -> None:
        # Update title
        self.title_row.set_title(Markup.find_url(Markup.escape(self.task_data.text)))

        # Update subtitle
        n_total, n_completed = self.get_status()
        self.title_row.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}" if n_total > 0 else ""
        )

        # Update toolbar button
        self.toolbar_toggle_btn.set_active(self.task_data.toolbar_shown)

        # Update completion
        completed: bool = self.task_data.completed
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.just_added = True
            self.complete_btn.set_active(completed)
            self.just_added = False

        # Update separator
        self.task_lists_separator.get_child().set_visible(
            n_completed > 0 and n_completed != n_total
        )

    def update_tags_bar(self) -> None:
        tags: str = self.task_data.tags
        tags_list_text: list[str] = [tag.title for tag in self.tags]

        # Delete tags
        for tag in self.tags:
            if tag.title not in tags:
                self.tags_bar.remove(tag)

        # Add tags
        for tag in tags:
            if tag not in tags_list_text:
                self.add_tag(tag)

        self.tags_bar_rev.set_reveal_child(tags != [])

    def update_progress_bar(self) -> None:
        # Log.debug(f"Task '{self.uid}': Update progress bar")
        total, completed = self.get_status()
        pc: int = (
            completed / total * 100
            if total > 0
            else (100 if self.task_data.completed else 0)
        )
        if self.task_data.percent_complete != pc:
            self.update_props(["percent_complete", "synced"], [pc, False])

        self.progress_bar.set_fraction(pc / 100)
        self.progress_bar_rev.set_reveal_child(total > 0)

    def update_toolbar(self) -> None:
        # If toolbar exists then update it
        if self.toolbar:
            self.toolbar.update_ui()

    def update_tasks(self, update_tasks: bool = True) -> None:
        # Tasks
        sub_tasks_data: list[TaskData] = UserData.get_tasks_as_dicts(
            self.list_uid, self.uid
        )
        sub_tasks_data_uids: list[str] = [task.uid for task in sub_tasks_data]
        sub_tasks_widgets: list[Task] = self.tasks
        sub_tasks_widgets_uids: list[str] = [task.uid for task in sub_tasks_widgets]

        # Add sub tasks
        for task in sub_tasks_data:
            if task.uid not in sub_tasks_widgets_uids:
                self.add_task(task)

        for task in self.tasks:
            # Remove task
            if task.uid not in sub_tasks_data_uids:
                task.purge()
            # Move task to completed tasks
            elif task.task_data.completed and task in self.uncompleted_tasks:
                if (
                    len(self.uncompleted_tasks) > 1
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.uncompleted_task_list.remove(task)
                self.completed_task_list.prepend(task)
            # Move task to uncompleted tasks
            elif not task.task_data.completed and task in self.completed_tasks:
                if (
                    len(self.uncompleted_tasks) > 0
                    and task.uid != self.uncompleted_tasks[-1].uid
                ):
                    UserData.move_task_after(
                        self.list_uid, task.uid, self.uncompleted_tasks[-1].uid
                    )
                self.completed_task_list.remove(task)
                self.uncompleted_task_list.append(task)

    def update_color(self) -> None:
        for cls in self.main_box.get_css_classes():
            if "task-" in cls:
                self.main_box.remove_css_class(cls)
                break
        for cls in self.complete_btn.get_css_classes():
            if "checkbtn-" in cls:
                self.complete_btn.remove_css_class(cls)
                break
        for cls in self.progress_bar.get_css_classes():
            if "progressbar-" in cls:
                self.progress_bar.remove_css_class(cls)
                break

        if color := self.task_data.color:
            self.main_box.add_css_class(f"task-{color}")
            self.complete_btn.add_css_class(f"checkbtn-{color}")
            self.progress_bar.add_css_class(f"progressbar-{color}")

    def update_ui(self) -> None:
        self.update_title()
        self.update_tags_bar()
        self.update_toolbar()
        self.update_progress_bar()
        self.update_tasks()
        self.update_color()

    # ------ SIGNAL HANDLERS ------ #

    def __on_right_click(self, _gesture_click, _n_press, x: int, y: int) -> None:
        position: Gdk.Rectangle = Gdk.Rectangle()
        position.x = x
        position.y = y
        self.popover_menu.set_pointing_to(position)
        self.popover_menu.popup()

    def _on_complete_btn_toggled(self, btn: Gtk.CheckButton) -> None:
        self.add_rm_crossline(btn.get_active())
        if self.block_signals:
            return

        Log.debug(f"Task '{self.uid}': Set completed to '{btn.get_active()}'")

        # Change prop
        self.update_props(["completed", "synced"], [btn.get_active(), False])

        # Move section
        self.get_parent().remove(self)
        if btn.get_active():
            self.parent.completed_task_list.prepend(self)
        else:
            self.parent.uncompleted_task_list.append(self)

        # Complete all sub-tasks if toggle is active
        if btn.get_active():
            for task in self.uncompleted_tasks:
                task.can_sync = False
                task.complete_btn.set_active(True)
                task.can_sync = True

        # Uncomplete parents if sub-task is uncompleted
        else:
            for task in self.parents_tree:
                if task.complete_btn.get_active():
                    task.can_sync = False
                    task.complete_btn.set_active(False)
                    task.can_sync = True

        # Update parent Task
        if isinstance(self.parent, Task):
            self.parent.update_progress_bar()
            self.parent.update_title()

        # Update parent TaskList
        self.task_list.update_title()

        self.update_title()
        self.update_progress_bar()
        State.tasks_page.update_status()
        if self.can_sync:
            Sync.sync()

    def _on_edit_row_applied(self, entry: Adw.EntryRow) -> None:
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.task_data.text:
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_title()
        Sync.sync()

    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.edit_row.props.text = ""
        self.edit_row.emit("apply")

    def _on_toolbar_toggle_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        if btn.get_active() != self.task_data.toolbar_shown:
            self.update_props(["toolbar_shown"], [btn.get_active()])
            # Create toolbar if needed
            if not self.toolbar and btn.get_active():
                self.__build_toolbar()
                self.update_toolbar()

    def _on_title_row_clicked(self, *args) -> None:
        self.expand(not self.sub_tasks.get_child_revealed())

    # --- DND --- #

    def _on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        # Bug workaround when task is not sensitive after short dnd
        for task in self.task_list.all_tasks:
            task.set_sensitive(True)
        self.set_sensitive(False)
        value: GObject.Value = GObject.Value(Task)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    def _on_drag_begin(self, _, drag) -> bool:
        text: str = self.task_data.text
        icon: Gtk.DragIcon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(Gtk.Button(label=text if len(text) < 20 else f"{text[0:20]}..."))

    def _on_drag_end(self, *_) -> bool:
        self.set_sensitive(True)
        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

    def _on_task_top_area_drop(self, _drop, task: Task, _x, _y) -> None:
        """When task is dropped on "+" area on top of task"""

        if task.get_next_sibling() == self:
            return

        task_data: TaskData = task.task_data
        task_parent: Task = task.parent
        old_task_list: TaskList = task.task_list
        task.purge()

        # Change data
        parent_changed: bool = False
        if task_data.list_uid == self.task_data.list_uid:
            if task_data.parent != self.task_data.parent:
                UserData.update_props(
                    self.list_uid,
                    task_data.uid,
                    ["parent", "synced"],
                    [self.task_data.parent, False],
                )
                parent_changed = True
        else:
            task_data: TaskData = UserData.move_task_to_list(
                task_data.uid, task_data.list_uid, self.list_uid, self.task_data.parent
            )
            parent_changed = True
            old_task_list.update_title()
        UserData.move_task_before(self.list_uid, task_data.uid, self.uid)

        if isinstance(task_parent, Task):
            task_parent.update_title()
            task_parent.update_progress_bar()

        # Move widget
        task_data.completed = self.task_data.completed
        new_task: Task = self.parent.add_task(task_data)
        self.get_parent().reorder_child_after(new_task, self)
        self.get_parent().reorder_child_after(self, new_task)

        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

        if isinstance(self.parent, Task):
            self.parent.update_title()
            self.parent.update_progress_bar()

        self.task_list.update_title()

        # Sync
        if parent_changed:
            Sync.sync()

    def _on_task_drop(self, _drop, task: Task, _x, _y) -> None:
        """When task is dropped on task and becomes sub-task"""

        if task.parent == self:
            return

        # Expand sub-tasks
        if not self.task_data.expanded:
            self.expand(True)

        task_data: TaskData = task.task_data
        task_parent: Task = task.parent
        old_task_list: TaskList = task.task_list
        task.purge()

        # Change parent
        parent_changed: bool = False
        if task_data.list_uid == self.list_uid:
            UserData.update_props(
                self.list_uid, task_data.uid, ["parent", "synced"], [self.uid, False]
            )
            parent_changed = True
        else:
            task_data: TaskData = UserData.move_task_to_list(
                task_data.uid, task_data.list_uid, self.list_uid, self.uid
            )
            parent_changed = True
            old_task_list.update_title()
        UserData.move_task_after(self.list_uid, task_data.uid, self.uid)
        self.add_task(task_data)
        if isinstance(task_parent, Task):
            task_parent.update_title()
            task_parent.update_progress_bar()

        if self.task_data.completed and not task_data.completed:
            self.complete_btn.set_active(False)
            for parent in self.parents_tree:
                parent.complete_btn.set_active(False)

        self.task_list.update_title()
        self.update_title()
        self.update_progress_bar()

        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

        # Sync
        if parent_changed:
            Sync.sync()

    def _on_sub_task_added(self, entry: Gtk.Entry) -> None:
        text: str = entry.get_text().strip()

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
        if self.task_data.completed:
            self.update_props(["completed", "synced"], [False, False])

        self.update_title()
        self.update_progress_bar()
        self.task_list.update_title()

        # Sync
        Sync.sync()


class Tag(Gtk.Box):
    def __init__(self, title: str, task: Task):
        super().__init__()
        self.title: str = title
        self.task: Task = task
        self.tags_bar: Gtk.FlowBox = task.tags_bar
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        self.set_hexpand(False)
        self.set_valign(Gtk.Align.CENTER)
        self.add_css_class("tag")

        # Text
        text: Gtk.Label = Gtk.Label(
            label=self.title,
            css_classes=["caption-heading"],
            max_width_chars=15,
            ellipsize=3,
            hexpand=True,
            halign=Gtk.Align.START,
        )
        self.append(text)

        # Delete button
        self.append(
            ErrandsButton(
                icon_name="errands-close-symbolic",
                cursor=Gdk.Cursor(name="pointer"),
                tooltip_text=_("Delete Tag"),
                on_click=self._on_delete_btn_clicked,
            )
        )

    # ------ SIGNAL HANDLERS ------ #

    def _on_delete_btn_clicked(self, _btn: Gtk.Button) -> None:
        Log.debug(f"Task '{self.task.uid}': Delete tag '{self.title}'")
        tags: list[str] = self.task.task_data.tags
        tags.remove(self.title)
        self.task.update_props(["tags", "synced"], [tags, False])
        self.task.update_tags_bar()
        Sync.sync()
