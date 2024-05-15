# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from gi.repository import Adw, Gio, GLib, GObject, Gtk  # type:ignore

from errands.lib.data import UserData
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children, get_human_datetime
from errands.state import State
from errands.widgets.shared.color_selector import ErrandsColorSelector
from errands.widgets.shared.components.boxes import ErrandsBox, ErrandsListBox
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsCheckButton
from errands.widgets.shared.titled_separator import TitledSeparator


if TYPE_CHECKING:
    from errands.widgets.task import Task


class ErrandsTaskToolbar(Gtk.FlowBox):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()

    def __build_ui(self) -> None:
        self.set_margin_bottom(2)
        self.set_margin_start(9)
        self.set_margin_end(9)
        self.set_max_children_per_line(2)
        self.set_selection_mode(Gtk.SelectionMode.NONE)

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
            on_click=lambda *_: State.datetime_window.show(self.task),
        )
        self.append(self.date_time_btn)

        # Notes button
        self.notes_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-notes-symbolic",
            tooltip_text=_("Notes"),
            css_classes=["flat"],
            on_click=lambda *_: State.notes_window.show(self.task),
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

        self.attachments_btn: ErrandsButton = ErrandsButton(
            tooltip_text=_("Attachments"),
            icon_name="errands-attachment-symbolic",
            css_classes=["flat"],
            on_click=lambda *_: State.attachments_window.show(self.task),
        )

        # Menu button
        menu_top_section: Gio.Menu = Gio.Menu()
        menu_colors_item: Gio.MenuItem = Gio.MenuItem()
        menu_colors_item.set_attribute_value("custom", GLib.Variant("s", "color"))
        menu_top_section.append_item(menu_colors_item)

        menu: Gio.Menu = Gio.Menu()
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
        self.color_selector: ErrandsColorSelector = ErrandsColorSelector()
        self.color_selector.connect("color-selected", self.__on_accent_color_selected)
        popover_menu.add_child(self.color_selector, "color")

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
            valign=Gtk.Align.CENTER,
            tooltip_text=_("More"),
        )
        menu_btn.connect("notify::active", self._on_menu_toggled)

        self.append(
            ErrandsBox(
                spacing=2,
                halign=Gtk.Align.END,
                children=[
                    self.notes_btn,
                    self.priority_btn,
                    self.tags_btn,
                    self.attachments_btn,
                    menu_btn,
                ],
            )
        )

    def update_ui(self):
        # Update Date and Time
        self.date_time_btn.get_child().props.label = get_human_datetime(
            self.task.task_data.due_date
        )
        self.date_time_btn.remove_css_class("error")
        if (
            self.task.task_data.due_date
            and datetime.fromisoformat(self.task.task_data.due_date).date()
            < datetime.today().date()
        ):
            self.date_time_btn.add_css_class("error")

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

        # Update attachments button css
        self.attachments_btn.remove_css_class("accent")
        if len(self.task.task_data.attachments) > 0:
            self.attachments_btn.add_css_class("accent")

    def __on_accent_color_selected(
        self, _color_selector, btn: ErrandsCheckButton, color: str
    ) -> None:
        if not btn.get_active() or self.task.block_signals:
            return
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
        self.task.block_signals = True
        self.color_selector.select_color(self.task.task_data.color)
        self.task.block_signals = False

    def _on_priority_btn_toggled(self, btn: Gtk.MenuButton, *_) -> None:
        priority: int = self.task.task_data.priority
        if btn.get_active():
            self.custom_priority_btn.set_value(priority)
        else:
            new_priority: int = self.custom_priority_btn.get_value_as_int()
            if priority != new_priority:
                Log.debug(f"Task Toolbar: Set priority to '{new_priority}'")
                self.task.update_props(["priority", "synced"], [new_priority, False])
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
        tags_list_items: list[ErrandsToolbarTagsListItem] = get_children(self.tags_list)
        tags_list_items_text = [t.title for t in tags_list_items]

        # Remove tags
        for item in tags_list_items:
            if item.title not in tags:
                self.tags_list.remove(item)

        # Add tags
        for tag in tags:
            if tag not in tags_list_items_text:
                self.tags_list.append(ErrandsToolbarTagsListItem(tag, self.task))

        # Toggle tags
        task_tags: list[str] = [t.title for t in self.task.tags]
        tags_items: list[ErrandsToolbarTagsListItem] = get_children(self.tags_list)
        for t in tags_items:
            t.block_signals = True
            t.toggle_btn.set_active(t.title in task_tags)
            t.block_signals = False

        self.tags_list.set_visible(len(get_children(self.tags_list)) > 0)


class ErrandsToolbarTagsListItem(Gtk.Box):
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
        Sync.sync()
