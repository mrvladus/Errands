# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from gi.repository import Adw, Gio, GLib, GObject, Gtk  # type:ignore

from errands.lib.data import UserData
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.widgets.shared.components.boxes import ErrandsBox, ErrandsListBox
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsCheckButton
from errands.widgets.shared.datetime_window import DateTimeWindow
from errands.widgets.shared.notes_window import NotesWindow
from errands.widgets.shared.titled_separator import TitledSeparator
from errands.widgets.task.tags_list_item import TagsListItem

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class TaskToolbar(Gtk.Revealer):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.update_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        self.task.title.toolbar_toggle_btn.bind_property(
            "active",
            self,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )

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
        self.datetime_window: DateTimeWindow = DateTimeWindow(self.task)

        # Notes button
        self.notes_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),
            css_classes=["flat"],
            on_click=self._on_notes_btn_clicked,
        )
        self.notes_window: NotesWindow = NotesWindow(self.task)

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
        self.tags_list: Gtk.Box = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=6,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
            margin_top=6,
        )
        tags_status_page: Adw.StatusPage = Adw.StatusPage(
            title=_("No Tags"),
            icon_name="errands-info-symbolic",
            css_classes=["compact"],
        )
        tags_status_page.bind_property(
            "visible",
            self.tags_list,
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

        menu.append(label=_("Edit"), detailed_action="task_toolbar.edit")
        menu.append(
            label=_("Move to Trash"),
            detailed_action="task_toolbar.move_to_trash",
        )
        menu.append(
            label=_("Copy to Clipboard"),
            detailed_action="task_toolbar.copy_to_clipboard",
        )
        menu.append(label=_("Export"), detailed_action="task_toolbar.export")
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

        # Flow Box Container
        flow_box: Gtk.FlowBox = Gtk.FlowBox(
            margin_start=9,
            margin_end=9,
            margin_bottom=2,
            max_children_per_line=2,
            selection_mode=0,
        )
        flow_box.append(self.date_time_btn)
        flow_box.append(
            ErrandsBox(
                spacing=3,
                halign=Gtk.Align.END,
                children=[self.notes_btn, self.priority_btn, self.tags_btn, menu_btn],
            )
        )
        self.set_child(flow_box)

    # ------ PROPERTIES ------ #

    # ------ PUBLIC METHODS ------ #

    def update_ui(self) -> None:
        # Update Date and Time
        self.datetime_window.due_date_time.datetime = self.task.task_data.due_date
        self.date_time_btn.get_child().props.label = (
            f"{self.datetime_window.due_date_time.human_datetime}"
        )

        # Update notes button css
        if self.task.task_data.notes:
            self.notes_btn.add_css_class("accent")
        else:
            self.notes_btn.remove_css_class("accent")

        # Update priority button css
        priority: int = self.task.task_data.priority
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

    # ------ SIGNAL HANDLERS ------ #

    def _on_accent_color_selected(self, btn: Gtk.CheckButton) -> None:
        if not btn.get_active() or self.task.block_signals:
            return
        color: str = btn.get_name()
        Log.debug(f"Task: change color to '{color}'")
        if color != self.task.task_data.color:
            self.task.update_props(
                ["color", "synced"], [color if color != "none" else "", False]
            )
            self.task.update_color()
            Sync.sync()

    def _on_menu_toggled(self, btn: Gtk.MenuButton, active: bool) -> None:
        if not btn.get_active():
            return

        # Update dates
        created_date: str = datetime.fromisoformat(
            self.task.task_data.created_at
        ).strftime("%Y.%m.%d %H:%M:%S")
        changed_date: str = datetime.fromisoformat(
            self.task.task_data.changed_at
        ).strftime("%Y.%m.%d %H:%M:%S")
        self.created_label.set_label(_("Created:") + " " + created_date)
        self.changed_label.set_label(_("Changed:") + " " + changed_date)

        # Update color
        color: str = self.task.task_data.color
        for btn in self.accent_color_btns.children:
            btn_color = btn.get_name()
            if color == "":
                color = "none"
            if btn_color == color:
                self.task.block_signals = True
                btn.set_active(True)
                self.task.block_signals = False

    def _on_date_time_btn_clicked(self, btn: Gtk.Button) -> None:
        self.datetime_window.show()

    def _on_notes_btn_clicked(self, btn: Gtk.Button) -> None:
        self.notes_window.show()

    def _on_priority_btn_toggled(self, btn: Gtk.MenuButton, *_) -> None:
        priority: int = self.task.task_data.priority
        if btn.get_active():
            self.custom_priority_btn.set_value(priority)
        else:
            new_priority: int = self.custom_priority_btn.get_value_as_int()
            if priority != new_priority:
                Log.debug(f"Task Toolbar: Set priority to '{new_priority}'")
                self.task.update_props(["priority", "synced"], [new_priority, False])
                self.update_ui()
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
        task_tags: list[str] = [t.title for t in self.task.tags]
        tags_items: list[TagsListItem] = get_children(self.tags_list)
        for t in tags_items:
            t.block_signals = True
            t.toggle_btn.set_active(t.title in task_tags)
            t.block_signals = False

        self.tags_list.set_visible(len(get_children(self.tags_list)) > 0)
