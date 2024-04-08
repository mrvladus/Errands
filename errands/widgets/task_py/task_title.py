# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk, Gdk

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync  # type:ignore


if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class TaskTitle(Gtk.ListBox):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.update_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        from errands.widgets.task.task import Task

        self.add_css_class("transparent")
        self.add_css_class("rounded-corners")

        # Hover controller
        hover_ctrl: Gtk.EventControllerMotion = Gtk.EventControllerMotion()
        self.add_controller(hover_ctrl)

        # Click controller
        click_ctrl: Gtk.GestureClick = Gtk.GestureClick()
        click_ctrl.connect("released", self._on_title_row_clicked)
        self.add_controller(click_ctrl)

        # Drop controller
        drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
            type=Task, actions=Gdk.DragAction.MOVE
        )
        drop_ctrl.connect("drop", self._on_task_drop)
        self.add_controller(drop_ctrl)

        # Drag controller
        drag_ctrl: Gtk.DragSource = Gtk.DragSource(actions=Gdk.DragAction.MOVE)
        drag_ctrl.connect("prepare", self._on_drag_prepare)
        drag_ctrl.connect("drag-begin", self._on_drag_begin)
        drag_ctrl.connect("drag-cancel", self._on_drag_end)
        drag_ctrl.connect("drag-end", self._on_drag_end)
        self.add_controller(drag_ctrl)

        # Title row
        self.title_row = Adw.ActionRow(
            height_request=60,
            use_markup=True,
            tooltip_text=_("Toggle Sub-Tasks"),  # noqa: F821
            cursor=Gdk.Cursor(name="pointer"),
            css_classes=["transparent", "rounded-corners"],
        )
        self.append(self.title_row)

        # Complete button
        self.complete_btn: Gtk.CheckButton = Gtk.CheckButton(
            tooltip_text=_("Toggle Completion"),  # noqa: F821
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
            tooltip_text=_("Toggle Toolbar"),  # noqa: F821
            css_classes=["circular", "flat"],
        )
        self.toolbar_toggle_btn.connect("toggled", self._on_toolbar_toggle_btn_toggled)

        # Suffix box
        suffix_box: Gtk.Box = Gtk.Box(spacing=6)
        suffix_box.append(expand_indicator_rev)
        suffix_box.append(self.toolbar_toggle_btn)
        self.title_row.add_suffix(suffix_box)

        # Edit row
        self.edit_row: Adw.EntryRow = Adw.EntryRow(
            title=_("Edit"),  # noqa: F821
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
        self.append(self.edit_row)

        # Cancel edit button
        cancel_edit_btn: Gtk.Button = Gtk.Button(
            tooltip_text=_("Cancel"),  # noqa: F821
            valign=Gtk.Align.CENTER,
            icon_name="window-close-symbolic",
            css_classes=["circular"],
        )
        cancel_edit_btn.connect("clicked", self._on_cancel_edit_btn_clicked)
        self.edit_row.add_suffix(cancel_edit_btn)

    # ------ PUBLIC METHODS ------ #

    def update_ui(self) -> None:
        # Update title
        self.title_row.set_title(
            Markup.find_url(Markup.escape(self.task.task_data.text))
        )

        # Update subtitle
        total, completed = self.task.get_status()
        self.title_row.set_subtitle(
            _("Completed:") + f" {completed} / {total}" if total > 0 else ""  # noqa: F821
        )

    # ------ SIGNAL HANDLERS ------ #

    def _on_complete_btn_toggled(self, btn: Gtk.CheckButton) -> None:
        self.add_rm_crossline(btn.get_active())
        if self.task.just_added:
            return

        Log.debug(f"Task '{self.task.uid}': Set completed to '{btn.get_active()}'")

        if self.get_prop("completed") != btn.get_active():
            self.update_props(["completed", "synced"], [btn.get_active(), False])

        # Complete all sub-tasks
        if btn.get_active():
            for task in self.task.all_tasks:
                if not task.get_prop("completed"):
                    task.update_props(["completed", "synced"], [True, False])
                    task.just_added = True
                    task.complete_btn.set_active(True)
                    task.just_added = False

        # Uncomplete parent if sub-task is uncompleted
        else:
            for task in self.task.parents_tree:
                if task.get_prop("completed"):
                    task.update_props(["completed", "synced"], [False, False])
                    task.just_added = True
                    task.complete_btn.set_active(False)
                    task.just_added = False

        if isinstance(self.task.parent, Task):
            self.task.parent.update_ui()
        else:
            self.task.parent.update_ui(False)
        self.task.task_list.update_status()
        Sync.sync()

    def _on_edit_row_applied(self, entry: Adw.EntryRow) -> None:
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.task.task_data.text:
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_ui()
        Sync.sync()

    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.edit_row.props.text = ""
        self.edit_row.emit("apply")

    def _on_toolbar_toggle_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        if btn.get_active() != self.task.task_data.toolbar_shown:
            self.update_props(["toolbar_shown"], [btn.get_active()])

    def _on_title_row_clicked(self, *args) -> None:
        self.task.expand(not self.task.sub_tasks.get_child_revealed())

    # --- DND --- #

    def _on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        # Bug workaround when task is not sensitive after short dnd
        for task in self.task.task_list.all_tasks:
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
        for task in self.task.task_list.all_tasks:
            task.top_drop_area.set_reveal_child(False)
            task.set_sensitive(True)

    def _on_task_drop(self, _drop, task: Task, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """

        if task.parent == self:
            return

        UserData.update_props(
            self.task.list_uid, task.uid, ["parent", "synced"], [self.task.uid, False]
        )

        # Change list
        if task.list_uid != self.task.list_uid:
            UserData.move_task_to_list(
                task.uid, task.list_uid, self.task.list_uid, self.task.uid
            )

        # Add task
        data: TaskData = UserData.get_task(self.task.list_uid, task.uid)
        self.add_task(data)

        # Remove task
        if task.task_list != self.task.task_list:
            task.task_list.update_status()
        if task.parent != self.task.parent:
            task.parent.update_ui(False)
        task.purge()

        # Toggle completion
        if self.task.task_data.completed and not data.completed:
            self.update_props(["completed", "synced"], [False, False])
            self.just_added = True
            self.complete_btn.set_active(False)
            self.just_added = False
            for parent in self.task.parents_tree:
                if parent.task_data.completed:
                    parent.update_props(["completed", "synced"], [False, False])
                    parent.just_added = True
                    parent.complete_btn.set_active(False)
                    parent.just_added = False

        # Expand sub-tasks
        if not self.get_prop("expanded"):
            self.expand(True)

        self.task.task_list.update_status()
        self.update_ui(False)
        # Sync
        Sync.sync()
