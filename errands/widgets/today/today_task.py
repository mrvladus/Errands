# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations


from datetime import datetime
from typing import Any, TYPE_CHECKING
from gi.repository import Adw, Gtk, Gio, GLib, GObject  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children, get_human_datetime
from errands.state import State
from errands.widgets.shared.components.boxes import (
    ErrandsBox,
    ErrandsFlowBox,
    ErrandsListBox,
)
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsCheckButton
from errands.widgets.shared.titled_separator import TitledSeparator
from errands.widgets.task import TagsListItem, Task
from errands.widgets.task import Tag
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
        self.insert_action_group(name="today_task", group=self.group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction(name=name)
            action.connect("activate", callback)
            self.group.add_action(action)

        def __edit(*args):
            self.edit_row.set_text(self.task_data.text)
            self.edit_row.set_visible(True)
            self.edit_row.grab_focus()

        __create_action("edit", __edit)
        __create_action(
            "copy_to_clipboard",
            lambda *_: self.task.activate_action("task.copy_to_clipboard", None),
        )
        __create_action(
            "export",
            lambda *_: self.task.activate_action("task.export", None),
        )
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
            on_click=lambda *_: State.datetime_window.show(self),
        )

        # Notes button
        self.notes_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),
            css_classes=["flat"],
            on_click=lambda *_: State.notes_window.show(self),
        )

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

        menu.append(label=_("Edit"), detailed_action="today_task.edit")
        menu.append(
            label=_("Move to Trash"),
            detailed_action="today_task.move_to_trash",
        )
        menu.append(
            label=_("Copy to Clipboard"),
            detailed_action="today_task.copy_to_clipboard",
        )
        menu.append(label=_("Export"), detailed_action="today_task.export")
        menu.append_section(None, menu_top_section)

        menu_bottom_section: Gio.Menu = Gio.Menu()
        menu_created_item: Gio.MenuItem = Gio.MenuItem()
        menu_created_item.set_attribute_value("custom", GLib.Variant("s", "created"))
        menu_bottom_section.append_item(menu_created_item)
        menu_changed_item: Gio.MenuItem = Gio.MenuItem()
        menu_changed_item.set_attribute_value("custom", GLib.Variant("s", "changed"))
        menu_bottom_section.append_item(menu_changed_item)
        menu.append_section(None, menu_bottom_section)

        popover_menu: Gtk.PopoverMenu = Gtk.PopoverMenu(menu_model=menu)

        # Colors
        none_color_btn: ErrandsCheckButton = ErrandsCheckButton(
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
        self.created_label: Gtk.Label = Gtk.Label(
            label=_("Created:"),
            halign=Gtk.Align.START,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
            css_classes=["caption-heading"],
        )
        popover_menu.add_child(self.created_label, "created")

        # Changed label
        self.changed_label: Gtk.Label = Gtk.Label(
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
            css_classes=["flat", "circular"],
            valign=Gtk.Align.CENTER,
            tooltip_text=_("More"),
        )
        menu_btn.connect("notify::active", self._on_menu_toggled)
        self.title_row.add_suffix(menu_btn)

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
                # Tags
                self.tags_bar_rev,
                # Toolbar
                ErrandsFlowBox(
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
                            ],
                        ),
                    ],
                ),
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

    def update_toolbar(self) -> None:
        # Update Date and Time
        self.date_time_btn.get_child().props.label = get_human_datetime(
            self.task_data.due_date
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

    def update_color(self) -> None:
        for c in self.main_box.get_css_classes():
            if "task-" in c:
                self.main_box.remove_css_class(c)
                break
        if color := self.task_data.color:
            self.main_box.add_css_class(f"task-{color}")

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
            self.task.update_color()
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
                self.task.update_toolbar()
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
