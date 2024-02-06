# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

from errands.lib.gsettings import GSettings

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList
    from errands.widgets.task import Task

from datetime import datetime
from errands.utils.data import UserData
from icalendar import Calendar, Event
from errands.utils.functions import get_children
from errands.widgets.components import Box, Button, DateTime
from gi.repository import Adw, Gtk, Gdk, GObject, GtkSource  # type:ignore
from errands.utils.markup import Markup
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
        # Header Bar
        hb = Adw.HeaderBar(show_title=False, show_back_button=False)
        # Back button
        back_btn = Button(
            icon_name="go-previous-symbolic",
            on_click=lambda *_: self.split_view.set_show_sidebar(False),
            visible=False,
        )
        back_btn.bind_property(
            "visible",
            self.split_view,
            "collapsed",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        hb.pack_start(back_btn)
        # Delete button
        delete_btn = Button(
            icon_name="errands-trash-symbolic",
            on_click=self._on_delete_btn_clicked,
            tooltip_text=_("Delete"),
        )
        hb.pack_start(delete_btn)

        # Status
        self.status = Adw.StatusPage(
            icon_name="errands-info-symbolic",
            visible=True,
            vexpand=True,
            title=_("No Details"),
            description=_("Click on task to show more info"),
        )
        self.status.add_css_class("compact")
        self.status.bind_property(
            "visible",
            delete_btn,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        # Colors
        colors_box = Gtk.Box(halign="center", css_classes=["toolbar"])
        colors = ["", "blue", "green", "yellow", "orange", "red", "purple", "brown"]
        for color in colors:
            btn = Gtk.Button()
            if color == "":
                btn.set_icon_name("window-close-symbolic")
                btn.set_tooltip_text(_("Clear Style"))
            else:
                btn.add_css_class("accent-color-btn")
                btn.add_css_class(f"btn-{color}")
            btn.add_css_class("circular")
            btn.connect("clicked", self._on_style_selected, color)
            colors_box.append(btn)

        # Edit group
        edit_group = Adw.PreferencesGroup(title=_("Text"))
        # Edit entry
        self.edit_entry = Gtk.TextBuffer()
        self.edit_entry.connect("changed", self._on_text_changed)
        edit_entry_text_ovrl = Gtk.Overlay(
            child=Gtk.TextView(
                height_request=100,
                top_margin=12,
                bottom_margin=12,
                left_margin=12,
                right_margin=12,
                buffer=self.edit_entry,
                css_classes=["card"],
                wrap_mode=3,
            )
        )
        # Save button
        self.edit_entry_save_btn: Gtk.Button = Gtk.Button(
            icon_name="errands-select-symbolic",
            css_classes=["circular", "suggested-action"],
            halign=Gtk.Align.END,
            valign=Gtk.Align.END,
            margin_bottom=6,
            margin_end=6,
            tooltip_text=_("Save"),
            visible=False,
        )
        self.edit_entry_save_btn.connect("clicked", lambda *_: self.save())
        edit_entry_text_ovrl.add_overlay(self.edit_entry_save_btn)
        edit_group.add(edit_entry_text_ovrl)
        # Copy button
        text_copy_btn = Gtk.Button(
            icon_name="errands-copy-symbolic",
            valign="center",
            tooltip_text=_("Copy Text"),
            css_classes=["flat"],
        )
        text_copy_btn.connect("clicked", self._on_copy_clicked, self.edit_entry)
        edit_group.set_header_suffix(text_copy_btn)

        # Notes group
        notes_group = Adw.PreferencesGroup(title=_("Notes"))
        # Notes entry
        notes_source_view = GtkSource.View(
            height_request=200,
            top_margin=12,
            bottom_margin=12,
            left_margin=12,
            right_margin=12,
            wrap_mode=3,
            css_classes=["card"],
        )
        self.notes = notes_source_view.get_buffer()
        lm: GtkSource.LanguageManager = GtkSource.LanguageManager.get_default()
        self.notes.set_language(lm.get_language("markdown"))
        self.notes.connect("changed", self._on_notes_changed)
        notes_ovrl = Gtk.Overlay(child=notes_source_view)
        # Save button
        self.notes_save_btn: Gtk.Button = Gtk.Button(
            icon_name="errands-select-symbolic",
            css_classes=["circular", "suggested-action"],
            halign=Gtk.Align.END,
            valign=Gtk.Align.END,
            margin_bottom=6,
            margin_end=6,
            tooltip_text=_("Save"),
            visible=False,
        )
        self.notes_save_btn.connect("clicked", lambda *_: self.save())
        notes_ovrl.add_overlay(self.notes_save_btn)
        notes_group.add(notes_ovrl)

        # Copy button
        notes_copy_btn = Gtk.Button(
            icon_name="errands-copy-symbolic",
            valign="center",
            tooltip_text=_("Copy Text"),
            css_classes=["flat"],
        )
        notes_copy_btn.connect("clicked", self._on_copy_clicked, self.notes)
        notes_group.set_header_suffix(notes_copy_btn)

        # Properties group
        props_group = Adw.PreferencesGroup(title=_("Properties"))

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
        props_group.add(self.start_datetime_row)

        # End date row
        self.end_datetime_row = Adw.ActionRow(title=_("Not Set"), subtitle=_("Due"))
        self.end_datetime = DateTime()
        self.end_datetime.connect("changed", self._on_end_time_changed)
        self.end_datetime_row.add_suffix(
            Gtk.MenuButton(
                valign="center",
                icon_name="errands-calendar-symbolic",
                tooltip_text=_("Set Date"),
                popover=Gtk.Popover(child=self.end_datetime),
                css_classes=["flat"],
            )
        )
        props_group.add(self.end_datetime_row)

        # Complete % row
        percent_complete_row = Adw.ActionRow(title=_("Complete %"))
        self.percent_complete = Gtk.SpinButton(
            valign="center",
            adjustment=Gtk.Adjustment(lower=0, upper=100, step_increment=1),
        )
        self.percent_complete.connect("changed", self._on_percent_complete_changed)
        percent_complete_row.add_suffix(self.percent_complete)
        props_group.add(percent_complete_row)

        # Priority row
        priority_row = Adw.ActionRow(title=_("Priority"))
        self.priority = Gtk.SpinButton(
            valign="center",
            adjustment=Gtk.Adjustment(lower=0, upper=9, step_increment=1),
        )
        self.priority.connect("changed", self._on_priority_changed)
        priority_row.add_suffix(self.priority)
        props_group.add(priority_row)

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

        # Groups box
        p_box = Box(
            children=[
                colors_box,
                edit_group,
                notes_group,
                props_group,
                self.tags,
                misc_group,
            ],
            orientation="vertical",
            spacing=12,
            visible=False,
        )
        p_box.bind_property(
            "visible",
            self.status,
            "visible",
            GObject.BindingFlags.SYNC_CREATE
            | GObject.BindingFlags.INVERT_BOOLEAN
            | GObject.BindingFlags.BIDIRECTIONAL,
        )
        # Toolbar View
        toolbar_view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(
                child=Adw.Clamp(
                    child=Box(children=[self.status, p_box], orientation="vertical"),
                    margin_start=12,
                    margin_end=12,
                    margin_top=6,
                    margin_bottom=24,
                )
            )
        )
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def add_tag(self, text: str) -> None:
        if text == "":
            return
        tag = Adw.ActionRow(title=text)
        delete_btn = Gtk.Button(
            icon_name="window-close-symbolic",
            valign="center",
            css_classes=["flat", "circular"],
        )
        delete_btn.connect("clicked", self._on_tag_deleted, tag)
        tag.add_suffix(delete_btn)
        self.tags.add(tag)

    def save(self):
        Log.debug("Details: Save")
        self.edit_entry_save_btn.set_visible(False)
        self.notes_save_btn.set_visible(False)

        # Text
        old_text: str = self.parent.get_prop("text")
        text: str = self.edit_entry.props.text
        if text.strip(" \n\t") and text != old_text:
            self.parent.task_row.task_row.set_title(
                Markup.find_url(Markup.escape(text))
            )

        # Save props
        self.parent.update_props(
            [
                "text",
                "notes",
                "completed",
                "start_date",
                "end_date",
                "percent_complete",
                "priority",
                "synced",
            ],
            [
                text if text.strip(" \n\t") and text != old_text else old_text,
                self.notes.props.text,
                self.parent.task_row.complete_btn.get_active(),
                self.start_datetime.get_datetime(),
                self.end_datetime.get_datetime(),
                self.percent_complete.get_value_as_int(),
                self.priority.get_value_as_int(),
                False,
            ],
        )

        self.parent.update_ui()
        Sync.sync(False)

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
        # Notes
        self.notes.set_text(self.parent.get_prop("notes"))
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

    def _on_text_changed(self, buffer: Gtk.TextBuffer):
        if not self.can_sync:
            return
        if buffer.props.text != self.parent.get_prop("text"):
            if GSettings.get("sync-provider") != 0:
                self.edit_entry_save_btn.set_visible(True)
            else:
                self.save()

    def _on_notes_changed(self, buffer: GtkSource.Buffer):
        if not self.can_sync:
            return
        if buffer.props.text != self.parent.get_prop("notes"):
            if GSettings.get("sync-provider") != 0:
                self.notes_save_btn.set_visible(True)
            else:
                self.save()

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

    def _on_copy_clicked(self, _btn, buffer: Gtk.TextBuffer):
        Log.info("Details: Copy to clipboard")
        Gdk.Display.get_default().get_clipboard().set(buffer.props.text)
        self.task_list.window.add_toast(_("Copied to Clipboard"))

    def _on_delete_btn_clicked(self, _btn):
        self.parent.delete()
        self.status.set_visible(True)

    def _on_export(self, _btn):
        def _confirm(dialog, res):
            try:
                file = dialog.save_finish(res)
            except:
                Log.debug("List: Export cancelled")
                return

            Log.info(f"Task: Export '{self.parent.uid}'")

            task = [
                i
                for i in UserData.get_tasks_as_dicts(self.parent.list_uid)
                if i["uid"] == self.parent.uid
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

        dialog = Gtk.FileDialog(initial_name=f"{self.parent.uid}.ics")
        dialog.save(self.task_list.window, None, _confirm)

    def _on_style_selected(self, btn: Gtk.Button, color: str) -> None:
        """
        Apply accent color
        """
        for c in self.parent.main_box.get_css_classes():
            if "task-" in c:
                self.parent.main_box.remove_css_class(c)
                break
        if color != "":
            self.parent.main_box.add_css_class(f"task-{color}")
        self.parent.update_props(["color", "synced"], [color, False])
        Sync.sync()

    # --- Tags --- #

    def _on_tag_added(self, entry: Adw.EntryRow) -> None:
        text = entry.get_text().strip(" \n\t")
        if text == "":
            return
        Log.debug("Details: Add tag")
        self.add_tag(entry.get_text())
        entry.set_text("")
        self._save_tags()

    def _on_tag_deleted(self, btn, tag):
        Log.debug("Details: Remove tag")
        self.tags.remove(tag)
        self._save_tags()

    def _save_tags(self):
        self.parent.update_props(
            ["tags", "synced"],
            [
                ",".join(
                    [
                        r.get_title()
                        for r in get_children(self.tag_entry.get_parent())[1:]
                    ]
                ),
                False,
            ],
        )
        Sync.sync()
