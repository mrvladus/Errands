# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import datetime
from typing import TYPE_CHECKING, Any

from gi.repository import Adw, Gdk, Gio, GLib, GObject, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox, ErrandsListBox
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsCheckButton
from errands.widgets.shared.components.menus import ErrandsMenuItem, ErrandsSimpleMenu
from errands.widgets.shared.task_toolbar.toolbar import ErrandsTaskToolbar
from errands.widgets.task import Tag, Task
from errands.widgets.task_list.task_list import TaskList

if TYPE_CHECKING:
    from errands.widgets.today.today import Today


class TodayTask(Gtk.Revealer):
    block_signals: bool = True
    purging: bool = False

    def __init__(self, task_data: TaskData, today_page: Today) -> None:
        super().__init__()
        self.task_data = task_data
        self.list_uid = task_data.list_uid
        self.uid = task_data.uid
        self.today_page = today_page
        self.__build_ui()
        self.__add_actions()
        self.update_ui()
        self.block_signals = False

    def __repr__(self) -> str:
        return f"<class 'TodayTask' {self.task_data.uid}>"

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
        # --- TITLE --- #

        # Title row
        self.title_row = Adw.ActionRow(
            height_request=60,
            use_markup=True,
            css_classes=["transparent", "rounded-corners"],
        )

        # Complete button
        self.complete_btn: ErrandsCheckButton = ErrandsCheckButton(
            tooltip_text=_("Toggle Completion"),
            valign=Gtk.Align.CENTER,
            css_classes=["selection-mode"],
            on_toggle=self._on_complete_btn_toggled,
        )
        self.title_row.add_prefix(self.complete_btn)

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

        # --- TOOL BAR --- #

        self.toolbar: ErrandsTaskToolbar = ErrandsTaskToolbar(self)

        # --- MAIN BOX --- #

        self.main_box: ErrandsBox = ErrandsBox(
            orientation=Gtk.Orientation.VERTICAL,
            margin_start=12,
            margin_end=12,
            margin_bottom=6,
            margin_top=6,
            css_classes=["card", "fade"],
            children=[
                # Title box
                ErrandsListBox(
                    css_classes=["transparent", "rounded-corners"],
                    children=[self.title_row, self.edit_row],
                ),
                self.popover_menu,
                # Tags
                self.tags_bar_rev,
                # Toolbar
                self.toolbar,
            ],
        )

        self.set_child(self.main_box)

    # ------ PROPERTIES ------ #

    @property
    def task(self) -> Task:
        return State.get_task(self.task_data.list_uid, self.task_data.uid)

    @property
    def task_list(self) -> TaskList:
        return State.get_task_list(self.task_data.list_uid)

    @property
    def tags(self) -> list[Tag]:
        return [item.get_child() for item in get_children(self.tags_bar)]

    # ------ PUBLIC METHODS ------ #

    def add_rm_crossline(self, add: bool) -> None:
        if add:
            self.title_row.add_css_class("task-completed")
        else:
            self.title_row.remove_css_class("task-completed")

    def add_tag(self, tag: str) -> None:
        self.tags_bar.append(Tag(tag, self))

    def get_prop(self, prop: str) -> Any:
        return UserData.get_prop(self.list_uid, self.uid, prop)

    def get_status(self) -> tuple[int, int]:
        """Get total tasks and completed tasks tuple"""
        return UserData.get_status(self.list_uid, self.uid)

    def delete(self, *_) -> None:
        self.toggle_visibility(False)
        self.task.delete()
        self.today_page.update_status()

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
                values.append(datetime.datetime.now().strftime("%Y%m%dT%H%M%S"))
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
        self.title_row.set_subtitle(
            UserData.get_list_prop(self.task_data.list_uid, "name")
        )

        # Update completion
        completed: bool = self.task_data.completed
        self.add_rm_crossline(completed)
        if self.complete_btn.get_active() != completed:
            self.block_signals = True
            self.complete_btn.set_active(completed)
            self.block_signals = False

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

        linked_task = State.get_task(self.task_data.list_uid, self.task_data.uid)
        if linked_task:
            linked_task.update_tags_bar()

    def update_toolbar(self) -> None:
        self.toolbar.update_ui()
        linked_task = State.get_task(self.task_data.list_uid, self.task_data.uid)
        if linked_task:
            linked_task.update_toolbar()

    def update_color(self) -> None:
        for cls in self.main_box.get_css_classes():
            if "task-" in cls:
                self.main_box.remove_css_class(cls)
                break
        for cls in self.complete_btn.get_css_classes():
            if "checkbtn-" in cls:
                self.complete_btn.remove_css_class(cls)
                break

        if color := self.task_data.color:
            self.main_box.add_css_class(f"task-{color}")
            self.complete_btn.add_css_class(f"checkbtn-{color}")

    def update_ui(self) -> None:
        self.toggle_visibility(not self.task_data.trash)
        self.update_color()
        self.update_title()
        self.update_tags_bar()
        self.update_toolbar()

    # ------ SIGNAL HANDLERS ------ #

    def _on_complete_btn_toggled(self, btn: Gtk.CheckButton) -> None:
        self.add_rm_crossline(btn.get_active())
        if self.block_signals:
            return

        self.task.complete_btn.set_active(btn.get_active())
        if btn.get_active():
            self.purge()
            State.today_page.update_status()

    def _on_edit_row_applied(self, entry: Adw.EntryRow) -> None:
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.task_data.text:
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_title()
        self.task.update_title()
        Sync.sync()

    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.edit_row.props.text = ""
        self.edit_row.emit("apply")

    def __on_right_click(self, _gesture_click, _n_press, x: int, y: int) -> None:
        position: Gdk.Rectangle = Gdk.Rectangle()
        position.x = x
        position.y = y
        self.popover_menu.set_pointing_to(position)
        self.popover_menu.popup()
