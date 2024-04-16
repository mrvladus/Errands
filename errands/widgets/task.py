# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Any

from gi.repository import Adw, Gdk, Gio, GLib, GObject, Gtk  # type:ignore
from icalendar import Calendar, Event

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.shared.components.boxes import (
    ErrandsBox,
    ErrandsFlowBox,
    ErrandsListBox,
)
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsCheckButton
from errands.widgets.shared.components.entries import ErrandsEntry
from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow
from errands.widgets.shared.titled_separator import TitledSeparator

if TYPE_CHECKING:
    from errands.widgets.task_list.task_list import TaskList


class Task(Gtk.Revealer):
    block_signals: bool = True

    def __init__(self, task_data: TaskData, parent: TaskList | Task) -> None:
        super().__init__()
        self.task_data = task_data
        self.list_uid = task_data.list_uid
        self.uid = task_data.uid
        self.parent = parent
        self.__build_ui()
        self.__load_sub_tasks()
        self.__add_actions()
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
                State.main_window.add_toast(_("Exported"))

            dialog = Gtk.FileDialog(initial_name=f"{self.uid}.ics")
            dialog.save(State.main_window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            Gdk.Display.get_default().get_clipboard().set(self.task_data.text)
            State.main_window.add_toast(_("Copied to Clipboard"))

        __create_action("edit", __edit)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("export", __export)
        __create_action("move_to_trash", lambda *_: self.delete())

    def __build_ui(self):
        # --- TOP DROP AREA --- #
        top_drop_area_img: Gtk.Image = Gtk.Image(
            icon_name="erraands-add-symbolic",
            margin_start=12,
            margin_end=12,
            css_classes=["task-drop-area"],
        )
        top_drop_area_drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
            type=Task, actions=Gdk.DragAction.MOVE
        )
        top_drop_area_drop_ctrl.connect("drop", self._on_task_drop)
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
        self.complete_btn: Gtk.CheckButton = Gtk.CheckButton(
            tooltip_text=_("Toggle Completion"),
            valign=Gtk.Align.CENTER,
            css_classes=["selection-mode"],
        )
        self.complete_btn.connect("toggled", self._on_complete_btn_toggled)
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
        self.toolbar_toggle_btn: Gtk.ToggleButton = Gtk.ToggleButton(
            icon_name="errands-toolbar-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Toggle Toolbar"),
            css_classes=["circular", "flat"],
        )
        self.toolbar_toggle_btn.connect("toggled", self._on_toolbar_toggle_btn_toggled)

        self.title_row.add_suffix(
            ErrandsBox(
                spacing=6, children=[expand_indicator_rev, self.toolbar_toggle_btn]
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
            css_classes=["osd", "dim-label"],
        )
        self.progress_bar_rev: Gtk.Revealer = Gtk.Revealer(child=self.progress_bar)

        # --- TOOL BAR --- #

        # Date and Time button
        self.date_time_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            halign=Gtk.Align.START,
            hexpand=True,
            tooltip_text=_("Start / Due Date"),
            css_classes=["flat", "caption"],
            child=Adw.ButtonContent(
                icon_name="errands-calendar-symbolic",
                can_shrink=True,
                label=_("Date"),
            ),
            on_click=self._on_date_time_btn_clicked,
        )
        self.datetime_window: DateTimeWindow = DateTimeWindow(self)

        # Notes button
        self.notes_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),
            css_classes=["flat"],
            on_click=self._on_notes_btn_clicked,
        )
        self.notes_window: NotesWindow = NotesWindow(self)

        # Priority button
        self.custom_priority_btn: Gtk.SpinButton = Gtk.SpinButton(
            valign=Gtk.Align.CENTER,
            adjustment=Gtk.Adjustment(upper=9, lower=0, step_increment=1),
        )
        self.priority_btn: Gtk.MenuButton = Gtk.MenuButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-priority-symbolic",
            tooltip_text=_("Priority"),
            css_classes=["flat"],
            popover=Gtk.Popover(
                css_classes=["menu"],
                child=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    margin_bottom=6,
                    margin_top=6,
                    margin_end=6,
                    margin_start=6,
                    spacing=3,
                    children=[
                        ErrandsListBox(
                            on_row_activated=self._on_priority_selected,
                            selection_mode=Gtk.SelectionMode.NONE,
                            children=[
                                Gtk.ListBoxRow(
                                    css_classes=["error"],
                                    child=Gtk.Label(label=_("High")),
                                ),
                                Gtk.ListBoxRow(
                                    css_classes=["warning"],
                                    child=Gtk.Label(label=_("Medium")),
                                ),
                                Gtk.ListBoxRow(
                                    css_classes=["accent"],
                                    child=Gtk.Label(label=_("Low")),
                                ),
                                Gtk.ListBoxRow(child=Gtk.Label(label=_("None"))),
                            ],
                        ),
                        TitledSeparator(title=_("Custom")),
                        self.custom_priority_btn,
                    ],
                ),
            ),
        )
        self.priority_btn.connect("notify::active", self._on_priority_btn_toggled)

        # Tags button
        tags_status_page: Adw.StatusPage = Adw.StatusPage(
            title=_("No Tags"),
            icon_name="errands-info-symbolic",
            css_classes=["compact"],
        )
        self.tags_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=6,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
            margin_top=6,
        )
        self.tags_list.bind_property(
            "visible",
            tags_status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        self.tags_btn: Gtk.MenuButton = Gtk.MenuButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-tag-add-symbolic",
            tooltip_text=_("Tags"),
            css_classes=["flat"],
            popover=Gtk.Popover(
                css_classes=["tags-menu"],
                child=Gtk.ScrolledWindow(
                    max_content_width=200,
                    max_content_height=200,
                    width_request=200,
                    propagate_natural_height=True,
                    propagate_natural_width=True,
                    vexpand=True,
                    child=ErrandsBox(
                        orientation=Gtk.Orientation.VERTICAL,
                        vexpand=True,
                        valign=Gtk.Align.CENTER,
                        children=[self.tags_list, tags_status_page],
                    ),
                ),
            ),
        )
        self.tags_btn.connect("notify::active", self._on_tags_btn_toggled)

        # Menu button
        menu_top_section: Gio.Menu = Gio.Menu()
        menu_colors_item: Gio.MenuItem = Gio.MenuItem()
        menu_colors_item.set_attribute_value("custom", GLib.Variant("s", "color"))
        menu_top_section.append_item(menu_colors_item)

        menu: Gio.Menu = Gio.Menu()

        menu.append(label=_("Edit"), detailed_action="task.edit")
        menu.append(
            label=_("Move to Trash"),
            detailed_action="task.move_to_trash",
        )
        menu.append(
            label=_("Copy to Clipboard"),
            detailed_action="task.copy_to_clipboard",
        )
        menu.append(label=_("Export"), detailed_action="task.export")
        menu.append_section(None, menu_top_section)

        menu_bottom_section: Gio.Menu = Gio.Menu()
        menu_created_item: Gio.MenuItem = Gio.MenuItem()
        menu_created_item.set_attribute_value("custom", GLib.Variant("s", "created"))
        menu_bottom_section.append_item(menu_created_item)
        menu_changed_item: Gio.MenuItem = Gio.MenuItem()
        menu_changed_item.set_attribute_value("custom", GLib.Variant("s", "changed"))
        menu_bottom_section.append_item(menu_changed_item)
        menu.append_section(None, menu_bottom_section)

        popover_menu = Gtk.PopoverMenu(menu_model=menu)

        # Colors
        none_color_btn = ErrandsCheckButton(
            on_toggle=self._on_accent_color_selected,
            name="none",
            css_classes=["accent-color-btn", "accent-color-btn-none"],
            tooltip_text=_("None"),
        )
        self.accent_color_btns = ErrandsBox(
            children=[
                none_color_btn,
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="blue",
                    css_classes=["accent-color-btn", "accent-color-btn-blue"],
                    tooltip_text=_("Blue"),
                ),
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="green",
                    css_classes=["accent-color-btn", "accent-color-btn-green"],
                    tooltip_text=_("Green"),
                ),
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="yellow",
                    css_classes=["accent-color-btn", "accent-color-btn-yellow"],
                    tooltip_text=_("Yellow"),
                ),
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="orange",
                    css_classes=["accent-color-btn", "accent-color-btn-orange"],
                    tooltip_text=_("Orange"),
                ),
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="red",
                    css_classes=["accent-color-btn", "accent-color-btn-red"],
                    tooltip_text=_("Red"),
                ),
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="purple",
                    css_classes=["accent-color-btn", "accent-color-btn-purple"],
                    tooltip_text=_("Purple"),
                ),
                ErrandsCheckButton(
                    group=none_color_btn,
                    on_toggle=self._on_accent_color_selected,
                    name="brown",
                    css_classes=["accent-color-btn", "accent-color-btn-brown"],
                    tooltip_text=_("Brown"),
                ),
            ]
        )
        popover_menu.add_child(self.accent_color_btns, "color")

        # Created label
        self.created_label = Gtk.Label(
            label=_("Created:"),
            halign=Gtk.Align.START,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
            css_classes=["caption-heading"],
        )
        popover_menu.add_child(self.created_label, "created")

        # Changed label
        self.changed_label = Gtk.Label(
            label=_("Changed:"),
            halign=Gtk.Align.START,
            margin_end=6,
            margin_start=6,
            css_classes=["caption-heading"],
        )
        popover_menu.add_child(self.changed_label, "changed")

        menu_btn: Gtk.MenuButton = Gtk.MenuButton(
            popover=popover_menu,
            icon_name="errands-more-symbolic",
            css_classes=["flat"],
        )
        menu_btn.connect("notify::active", self._on_menu_toggled)

        toolbar: Gtk.Revealer = Gtk.Revealer(
            child=ErrandsFlowBox(
                margin_start=9,
                margin_end=9,
                margin_bottom=2,
                max_children_per_line=2,
                selection_mode=0,
                children=[
                    self.date_time_btn,
                    ErrandsBox(
                        spacing=3,
                        halign=Gtk.Align.END,
                        children=[
                            self.notes_btn,
                            self.priority_btn,
                            self.tags_btn,
                            menu_btn,
                        ],
                    ),
                ],
            )
        )
        self.toolbar_toggle_btn.bind_property(
            "active",
            toolbar,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )

        # --- SUB TASKS --- #

        # Uncompleted tasks
        self.uncompleted_task_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
        )

        # Completed tasks
        self.completed_task_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
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
                self.tags_bar_rev,
                self.progress_bar_rev,
                toolbar,
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

    def __load_sub_tasks(self) -> None:
        tasks: list[TaskData] = (
            t
            for t in UserData.get_tasks_as_dicts(self.list_uid, self.uid)
            if not t.deleted
        )
        for task in tasks:
            new_task = Task(task, self)
            if task.completed:
                self.completed_task_list.append(new_task)
            else:
                self.uncompleted_task_list.append(new_task)

        self.set_reveal_child(self.task_data.expanded)
        self.toggle_visibility(not self.task_data.trash)
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
        Log.info(f"Task '{self.uid}': Add task '{task.uid}'")

        on_top: bool = GSettings.get("task-list-new-task-position-top")
        new_task = Task(task, self)
        if on_top:
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
        self.just_added = True
        self.complete_btn.set_active(True)
        self.just_added = False
        self.update_props(["trash", "completed", "synced"], [True, True, False])
        for task in self.all_tasks:
            task.just_added = True
            task.complete_btn.set_active(True)
            task.just_added = False
            task.update_props(["trash", "completed", "synced"], [True, True, False])
        State.today_page.update_ui()
        State.trash_sidebar_row.update_ui()
        State.tags_page.update_ui()
        self.task_list.update_status()

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
        )
        for prop in props:
            if prop not in local_props:
                props.append("changed_at")
                values.append(datetime.now().strftime("%Y%m%dT%H%M%S"))
                break
        UserData.update_props(self.list_uid, self.uid, props, values)
        # Update linked today task
        if props == ["expanded"] or props == ["toolbar_shown"]:
            State.today_page.update_ui()

    def toggle_visibility(self, on: bool) -> None:
        GLib.idle_add(self.set_reveal_child, on)

    def update_title(self) -> None:
        # Update title
        self.title_row.set_title(Markup.find_url(Markup.escape(self.task_data.text)))

        # Update subtitle
        total, completed = self.get_status()
        self.title_row.set_subtitle(
            _("Completed:") + f" {completed} / {total}" if total > 0 else ""
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
        total, completed = self.get_status()
        pc: int = (
            completed / total * 100
            if total > 0
            else (100 if self.complete_btn.get_active() else 0)
        )
        if self.task_data.percent_complete != pc:
            self.update_props(["percent_complete", "synced"], [pc, False])

        self.progress_bar.set_fraction(pc / 100)
        self.progress_bar_rev.set_reveal_child(total > 0)

    def update_toolbar(self) -> None:
        # Update Date and Time
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

    def update_tasks(self, update_tasks: bool = True) -> None:
        # Expand
        self.set_reveal_child(self.task_data.expanded)

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

        # Update tasks
        if update_tasks:
            for task in self.tasks:
                task.update_ui()

    def update_color(self) -> None:
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        if color := self.task_data.color:
            self.main_box.add_css_class(f"task-{color}")

    def update_ui(self) -> None:
        self.update_title()
        self.update_tags_bar()
        self.update_progress_bar()
        self.update_tasks()
        self.update_color()

    # ------ SIGNAL HANDLERS ------ #

    def _on_task_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on "+" area on top of task
        """

        if task.list_uid != self.list_uid:
            UserData.move_task_to_list(
                task.uid,
                task.list_uid,
                self.list_uid,
                self.parent.uid if isinstance(self.parent, Task) else "",
            )
        UserData.move_task_before(self.list_uid, task.uid, self.uid)

        # If task completed and self is not completed - uncomplete task
        if task.complete_btn.get_active() and not self.complete_btn.get_active():
            task.update_props(["completed"], [False])
        elif not task.complete_btn.get_active() and self.complete_btn.get_active():
            task.update_props(["completed"], [True])

        # If task has the same parent box
        if task.parent == self.parent:
            box: Gtk.Box = self.get_parent()
            box.reorder_child_after(task, self)
            box.reorder_child_after(self, task)

        # Change parent if different parents
        else:
            UserData.update_props(
                self.list_uid,
                task.uid,
                ["parent", "synced"],
                [
                    self.parent.uid if isinstance(self.parent, Task) else "",
                    False,
                ],
            )

            # Toggle completion for parents
            if not task.get_prop("completed"):
                for parent in self.parents_tree:
                    parent.complete_btn.set_active(False)
            task.purge()

            new_task: Task = Task(
                UserData.get_task(self.list_uid, task.uid),
                self.task_list,
                self.parent,
            )
            box: Gtk.Box = self.get_parent()
            box.append(new_task)
            box.reorder_child_after(new_task, self)
            box.reorder_child_after(self, new_task)

        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

        # Update UI
        self.task_list.update_status()

        # Sync
        Sync.sync()

    def _on_complete_btn_toggled(self, btn: Gtk.CheckButton) -> None:
        from errands.widgets.task import Task

        self.add_rm_crossline(btn.get_active())
        if self.block_signals:
            return

        Log.debug(f"Task '{self.uid}': Set completed to '{btn.get_active()}'")

        if self.task_data.completed != btn.get_active():
            self.update_props(["completed", "synced"], [btn.get_active(), False])

        # Complete all sub-tasks
        if btn.get_active():
            for task in self.all_tasks:
                if not task.task_data.completed:
                    task.update_props(["completed", "synced"], [True, False])
                    task.block_signals = True
                    task.complete_btn.set_active(True)
                    task.block_signals = False

        # Uncomplete parent if sub-task is uncompleted
        else:
            for task in self.parents_tree:
                if task.task_data.completed:
                    task.update_props(["completed", "synced"], [False, False])
                    task.block_signals = True
                    task.complete_btn.set_active(False)
                    task.block_signals = False

        if isinstance(self.parent, Task):
            self.parent.update_ui()
        else:
            self.parent.update_ui(False)
        self.task_list.update_status()
        Sync.sync()

    def _on_edit_row_applied(self, entry: Adw.EntryRow) -> None:
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.task_data.text:
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_ui()
        Sync.sync()

    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.edit_row.props.text = ""
        self.edit_row.emit("apply")

    def _on_toolbar_toggle_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        if btn.get_active() != self.task_data.toolbar_shown:
            self.update_props(["toolbar_shown"], [btn.get_active()])

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
        text: str = self.get_prop("text")
        icon: Gtk.DragIcon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(Gtk.Button(label=text if len(text) < 20 else f"{text[0:20]}..."))

    def _on_drag_end(self, *_) -> bool:
        self.set_sensitive(True)
        # KDE dnd bug workaround for issue #111
        for task in self.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

    def _on_task_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """

        if task.parent == self:
            return

        UserData.update_props(
            self.list_uid, task.uid, ["parent", "synced"], [self.task.uid, False]
        )

        # Change list
        if task.list_uid != self.list_uid:
            UserData.move_task_to_list(task.uid, task.list_uid, self.list_uid, self.uid)

        # Add task
        data: TaskData = UserData.get_task(self.list_uid, task.uid)
        self.add_task(data)

        # Remove task
        if task.task_list != self.task_list:
            task.task_list.update_status()
        if task.parent != self.parent:
            task.parent.update_ui(False)
        task.purge()

        # Toggle completion
        if self.task_data.completed and not data.completed:
            self.update_props(["completed", "synced"], [False, False])
            self.just_added = True
            self.complete_btn.set_active(False)
            self.just_added = False
            for parent in self.parents_tree:
                if parent.task_data.completed:
                    parent.update_props(["completed", "synced"], [False, False])
                    parent.just_added = True
                    parent.complete_btn.set_active(False)
                    parent.just_added = False

        # Expand sub-tasks
        if not self.get_prop("expanded"):
            self.expand(True)

        self.task_list.update_status()
        self.update_ui(False)
        # Sync
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
        self.task_list.update_status()

        # Sync
        Sync.sync()

    # --- TOOLBAR SIGNALS --- #

    def _on_accent_color_selected(self, btn: Gtk.CheckButton) -> None:
        if not btn.get_active() or self.block_signals:
            return
        color: str = btn.get_name()
        Log.debug(f"Task: change color to '{color}'")
        if color != self.task_data.color:
            self.update_props(
                ["color", "synced"], [color if color != "none" else "", False]
            )
            self.update_color()
            Sync.sync()

    def _on_menu_toggled(self, btn: Gtk.MenuButton, active: bool) -> None:
        if not btn.get_active():
            return

        # Update dates
        created_date: str = datetime.fromisoformat(self.task_data.created_at).strftime(
            "%Y.%m.%d %H:%M:%S"
        )
        changed_date: str = datetime.fromisoformat(self.task_data.changed_at).strftime(
            "%Y.%m.%d %H:%M:%S"
        )
        self.created_label.set_label(_("Created:") + " " + created_date)
        self.changed_label.set_label(_("Changed:") + " " + changed_date)

        # Update color
        color: str = self.task_data.color
        for btn in self.accent_color_btns.children:
            btn_color = btn.get_name()
            if color == "":
                color = "none"
            if btn_color == color:
                self.block_signals = True
                btn.set_active(True)
                self.block_signals = False

    def _on_date_time_btn_clicked(self, btn: Gtk.Button) -> None:
        self.datetime_window.show()

    def _on_notes_btn_clicked(self, btn: Gtk.Button) -> None:
        self.notes_window.show()

    def _on_priority_btn_toggled(self, btn: Gtk.MenuButton, *_) -> None:
        priority: int = self.task_data.priority
        if btn.get_active():
            self.custom_priority_btn.set_value(priority)
        else:
            new_priority: int = self.custom_priority_btn.get_value_as_int()
            if priority != new_priority:
                Log.debug(f"Task Toolbar: Set priority to '{new_priority}'")
                self.update_props(["priority", "synced"], [new_priority, False])
                self.update_toolbar()
                Sync.sync()

    def _on_priority_selected(self, box: Gtk.ListBox, row: Gtk.ListBoxRow) -> None:
        rows: list[Gtk.ListBoxRow] = get_children(box)
        for i, r in enumerate(rows):
            if r == row:
                index = i
                break
        match index:
            case 0:
                self.custom_priority_btn.set_value(1)
            case 1:
                self.custom_priority_btn.set_value(5)
            case 2:
                self.custom_priority_btn.set_value(9)
            case 3:
                self.custom_priority_btn.set_value(0)
        self.priority_btn.popdown()

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


class TagsListItem(Gtk.Box):
    block_signals = False

    def __init__(self, title: str, task: Task) -> None:
        super().__init__()
        self.set_spacing(6)
        self.title = title
        self.task = task
        self.toggle_btn = ErrandsCheckButton(on_toggle=self.__on_toggle)
        self.append(self.toggle_btn)
        self.append(
            Gtk.Label(
                label=title, hexpand=True, halign=Gtk.Align.START, max_width_chars=20
            )
        )
        self.append(
            Gtk.Image(icon_name="errands-tag-symbolic", css_classes=["dim-label"])
        )

    def __on_toggle(self, btn: Gtk.CheckButton) -> None:
        if self.block_signals:
            return

        tags: list[str] = self.task.task_data.tags

        if btn.get_active():
            if self.title not in tags:
                tags.append(self.title)
        else:
            if self.title in tags:
                tags.remove(self.title)

        self.task.update_props(["tags", "synced"], [tags, False])
        self.task.update_tags_bar()


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
        tags: list[str] = self.task.task_data.tags
        tags.remove(self.title)
        self.task.update_props(["tags", "synced"], [tags, False])
        self.task.update_tags_bar()
        Sync.sync()
