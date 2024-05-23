# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from gi.repository import Adw, Gio, GLib, GObject, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_human_datetime
from errands.state import State
from errands.widgets.shared.color_selector import ErrandsColorSelector
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsCheckButton


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
        self.priority_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-priority-symbolic",
            tooltip_text=_("Priority"),
            css_classes=["flat"],
            on_click=lambda *_: State.priority_window.show(self.task),
        )

        # Tags button
        tag_btn: ErrandsButton = ErrandsButton(
            valign=Gtk.Align.CENTER,
            icon_name="errands-tag-add-symbolic",
            tooltip_text=_("Tags"),
            css_classes=["flat"],
            on_click=lambda *_: State.tag_window.show(self.task),
        )

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
        self.color_selector: ErrandsColorSelector = ErrandsColorSelector(
            on_color_selected=self.__on_accent_color_selected
        )
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
                    tag_btn,
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
