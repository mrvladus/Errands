# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

from errands.lib.gsettings import GSettings

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList
    from errands.widgets.task.task import Task

from datetime import datetime
from errands.lib.data import UserData
from icalendar import Calendar, Event
from errands.lib.utils import get_children
from errands.widgets.components import Box, Button, DateTime
from gi.repository import Adw, Gtk, Gdk, GObject, GtkSource, Gio  # type:ignore
from errands.lib.markup import Markup
from errands.lib.sync.sync import Sync
from errands.lib.logging import Log


class Details(Adw.Bin):
    parent: Task = None
    can_sync: bool = False

    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.split_view: Adw.OverlaySplitView = task_list.split_view
        self._build_ui()

    def _build_ui(self):
        # Start date row
        self.start_datetime_row = Adw.ActionRow(title=_("Not Set"), subtitle=_("Start"))
        self.start_datetime = DateTime()
        self.start_datetime.connect("changed", self._on_start_time_changed)
        self.start_datetime_row.add_suffix(
            Gtk.MenuButton(
                valign="center",
                icon_name="errands-calendar-symbolic",
                tooltip_text=_("Set Date"),
                popover=Gtk.Popover(
                    child=Gtk.ScrolledWindow(
                        propagate_natural_height=True,
                        propagate_natural_width=True,
                        child=self.start_datetime,
                    )
                ),
                css_classes=["flat"],
            )
        )

        # End date row
        self.end_datetime_row = Adw.ActionRow(title=_("Not Set"), subtitle=_("Due"))
        self.end_datetime = DateTime()
        self.end_datetime.connect("changed", self._on_end_time_changed)
        self.end_datetime_row.add_suffix(
            Gtk.MenuButton(
                valign="center",
                icon_name="errands-calendar-symbolic",
                tooltip_text=_("Set Date"),
                popover=Gtk.Popover(
                    child=Gtk.ScrolledWindow(
                        propagate_natural_height=True,
                        propagate_natural_width=True,
                        child=self.end_datetime,
                    )
                ),
                css_classes=["flat"],
            )
        )

        # Complete % row
        percent_complete_row = Adw.ActionRow(title=_("Complete %"))
        self.percent_complete = Gtk.SpinButton(
            valign="center",
            adjustment=Gtk.Adjustment(lower=0, upper=100, step_increment=1),
        )
        self.percent_complete.connect("changed", self._on_percent_complete_changed)
        percent_complete_row.add_suffix(self.percent_complete)

        # Tags group
        self.tags = Adw.PreferencesGroup(title=_("Tags"))
        # Tags entry
        self.tag_entry = Adw.EntryRow(title=_("Add Tag"))
        self.tag_entry.connect("entry-activated", self._on_tag_added)
        self.tags.add(self.tag_entry)

        # Export group
        misc_group = Adw.PreferencesGroup(title=_("Export"))
        open_cal_btn = Button(
            icon_name="errands-share-symbolic",
            on_click=self._on_export,
            valign="center",
            css_classes=["flat"],
        )
        open_cal_row = Adw.ActionRow(
            title=_("Export"),
            subtitle=_("Save Task as .ics file"),
            activatable_widget=open_cal_btn,
        )
        open_cal_row.add_suffix(open_cal_btn)
        misc_group.add(open_cal_row)

    def update_info(self, parent: Task):
        # Show status on empty task
        if parent == None:
            self.status.set_visible(True)
            return

        self.can_sync = False
        self.parent = parent

        Log.debug("Details: Update info")

        # Edit text
        self.edit_entry.set_text(self.parent.get_prop("text"))
        # Datetime
        self.start_datetime.set_datetime(self.parent.get_prop("start_date"))
        self.start_datetime_row.set_title(self.start_datetime.get_human_datetime())
        self.end_datetime.set_datetime(self.parent.get_prop("end_date"))
        self.end_datetime_row.set_title(self.end_datetime.get_human_datetime())
        # Percent complete
        self.percent_complete.set_value(self.parent.get_prop("percent_complete"))
        # Priority
        self.priority.set_value(self.parent.get_prop("priority"))
        self.status.set_visible(False)
        # Tags
        # Remove old
        for i, tag in enumerate(get_children(self.tag_entry.get_parent())):
            # Skip entry
            if i == 0:
                continue
            self.tags.remove(tag)
        # Add new
        for tag in self.parent.get_prop("tags").split(","):
            self.add_tag(tag)
        self.can_sync = True

    def _on_percent_complete_changed(self, _):
        if not self.can_sync:
            return
        self.save()

    def _on_priority_changed(self, row: Adw.SpinRow):
        if not self.can_sync:
            return
        self.save()

    def _on_start_time_changed(self, *args):
        if not self.can_sync:
            return

        Log.debug("Details: change start time")

        sdt: int = self.start_datetime.get_datetime_as_int()
        edt: int = self.end_datetime.get_datetime_as_int()
        if sdt > edt and edt != 0:
            self.end_datetime.set_datetime(self.start_datetime.get_datetime())
            self.end_datetime_row.set_title(self.end_datetime.get_human_datetime())
        self.start_datetime_row.set_title(self.start_datetime.get_human_datetime())
        self.save()

    def _on_end_time_changed(self, *args):
        if not self.can_sync:
            return

        Log.debug("Details: change end time")

        sdt: int = self.start_datetime.get_datetime_as_int()
        edt: int = self.end_datetime.get_datetime_as_int()
        if edt < sdt and sdt != 0 and edt != 0:
            self.start_datetime.set_datetime(self.end_datetime.get_datetime())
            self.start_datetime_row.set_title(self.start_datetime.get_human_datetime())
        self.end_datetime_row.set_title(self.end_datetime.get_human_datetime())
        self.save()
