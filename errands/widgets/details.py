# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from datetime import datetime
from errands.utils.data import UserData
from icalendar import Calendar, Event

from errands.utils.functions import get_children
from errands.widgets.components.datetime import DateTime
from gi.repository import Adw, Gtk, GLib, Gdk, GObject
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from errands.utils.logging import Log


class Details(Adw.Bin):
    parent = None

    def __init__(self, window, task_list) -> None:
        super().__init__()
        self.window = window
        self.task_list = task_list
        self.build_ui()

    def build_ui(self):
        # Header Bar
        hb = Adw.HeaderBar(show_title=False, show_back_button=False)
        # Back button
        back_btn = Gtk.Button(icon_name="go-previous-symbolic", visible=False)
        back_btn.bind_property(
            "visible",
            self.task_list.split_view,
            "collapsed",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        back_btn.connect(
            "clicked", lambda *_: self.task_list.split_view.set_show_sidebar(False)
        )
        hb.pack_start(back_btn)
        # Delete button
        delete_btn = Gtk.Button(
            icon_name="user-trash-symbolic",
            tooltip_text=_("Delete"),  # type:ignore
        )
        delete_btn.connect("clicked", self.on_delete_btn_clicked)
        hb.pack_start(delete_btn)
        # Save button
        self.save_btn = Gtk.Button(
            label=_("Save"),  # type:ignore
            css_classes=["suggested-action"],
        )
        self.save_btn.connect("clicked", self.on_save_btn_clicked)
        hb.pack_end(self.save_btn)
        # Status
        self.status = Adw.StatusPage(
            icon_name="help-about-symbolic",
            visible=True,
            vexpand=True,
            title=_("No Details"),  # type:ignore
            description=_("Click on task to show more info"),  # type:ignore
        )
        self.status.add_css_class("compact")
        self.status.bind_property(
            "visible",
            delete_btn,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        self.status.bind_property(
            "visible",
            self.save_btn,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        # Colors
        colors_box = Gtk.Box(halign="center", css_classes=["toolbar"])
        colors = ["", "blue", "green", "yellow", "orange", "red", "purple", "brown"]
        for color in colors:
            btn = Gtk.Button(can_shrink=True)
            if color == "":
                btn.set_icon_name("window-close-symbolic")
                btn.set_tooltip_text(_("Clear Style"))  # type:ignore
            else:
                btn.add_css_class("accent-color-btn")
                btn.add_css_class(f"btn-{color}")
            btn.add_css_class("circular")
            btn.connect("clicked", self.on_style_selected, color)
            colors_box.append(btn)
        # Edit group
        edit_group = Adw.PreferencesGroup(title=_("Text"))  # type:ignore
        # Copy button
        copy_btn = Gtk.Button(
            icon_name="errands-copy",
            valign="center",
            css_classes=["flat"],
            tooltip_text=_("Copy Text"),  # type:ignore
        )
        copy_btn.connect("clicked", self.on_copy_text_clicked)
        edit_group.set_header_suffix(copy_btn)
        # Edit entry
        self.edit_entry = Gtk.TextBuffer()
        self.edit_entry.connect("changed", lambda *_: self.save_btn.set_sensitive(True))
        edit_view = Gtk.TextView(
            height_request=55,
            top_margin=12,
            bottom_margin=12,
            left_margin=12,
            right_margin=12,
            buffer=self.edit_entry,
        )
        edit_view.add_css_class("card")
        edit_group.add(edit_view)
        # Notes group
        notes_group = Adw.PreferencesGroup(title=_("Notes"))  # type:ignore
        # Notes entry
        self.notes = Gtk.TextBuffer()
        self.notes.connect("changed", lambda *_: self.save_btn.set_sensitive(True))
        notes_view = Gtk.TextView(
            height_request=100,
            top_margin=12,
            bottom_margin=12,
            left_margin=12,
            right_margin=12,
            buffer=self.notes,
            wrap_mode=3,
        )
        notes_view.add_css_class("card")
        notes_group.add(notes_view)

        # Properties group
        props_group = Adw.PreferencesGroup(title=_("Properties"))  # type:ignore

        # Start date row
        self.start_datetime_row = Adw.ActionRow(
            title=_("Not Set"), subtitle=_("Start")  # type:ignore
        )
        self.start_datetime = DateTime()
        self.start_datetime.connect("changed", self.on_start_time_changed)
        self.start_datetime_row.add_suffix(
            Gtk.MenuButton(
                valign="center",
                icon_name="errands-calendar",
                tooltip_text=_("Set Date"),  # type:ignore
                popover=Gtk.Popover(child=self.start_datetime),
                css_classes=["flat"],
            )
        )
        props_group.add(self.start_datetime_row)

        # End date row
        self.end_datetime_row = Adw.ActionRow(
            title=_("Not Set"), subtitle=_("Start")  # type:ignore
        )
        self.end_datetime = DateTime()
        self.end_datetime.connect("changed", self.on_end_time_changed)
        self.end_datetime_row.add_suffix(
            Gtk.MenuButton(
                valign="center",
                icon_name="errands-calendar",
                tooltip_text=_("Set Date"),  # type:ignore
                popover=Gtk.Popover(child=self.end_datetime),
                css_classes=["flat"],
            )
        )
        props_group.add(self.end_datetime_row)

        # Complete % row
        self.percent_complete = Adw.SpinRow(
            title=_("Complete %"),  # type:ignore
            adjustment=Gtk.Adjustment(lower=0, upper=100, step_increment=1),
        )
        self.percent_complete.connect(
            "changed", lambda *_: self.save_btn.set_sensitive(True)
        )
        props_group.add(self.percent_complete)

        # Priority row
        self.priority = Adw.SpinRow(
            title=_("Priority"),  # type:ignore
            adjustment=Gtk.Adjustment(lower=0, upper=9, step_increment=1),
        )
        self.priority.connect("changed", lambda *_: self.save_btn.set_sensitive(True))
        props_group.add(self.priority)

        # Tags group
        self.tags = Adw.PreferencesGroup(title=_("Tags"))  # type:ignore
        # Tags entry
        self.tag_entry = Adw.EntryRow(title=_("Add Tag"))  # type:ignore
        self.tag_entry.connect("entry-activated", self.on_tag_added)
        self.tags.add(self.tag_entry)

        # Export group
        misc_group = Adw.PreferencesGroup(title=_("Export"))  # type:ignore
        open_cal_btn = Gtk.Button(
            icon_name="errands-share",
            valign="center",
            css_classes=["flat"],
        )
        open_cal_btn.connect("clicked", self.on_export)
        open_cal_row = Adw.ActionRow(
            title=_("Export"),  # type:ignore
            subtitle=_("Save Task as .ics file"),  # type:ignore
            activatable_widget=open_cal_btn,
        )
        open_cal_row.add_suffix(open_cal_btn)
        misc_group.add(open_cal_row)

        # Groups box
        p_box = Gtk.Box(orientation="vertical", spacing=12, visible=False)
        p_box.bind_property(
            "visible",
            self.status,
            "visible",
            GObject.BindingFlags.SYNC_CREATE
            | GObject.BindingFlags.INVERT_BOOLEAN
            | GObject.BindingFlags.BIDIRECTIONAL,
        )
        p_box.append(colors_box)
        p_box.append(edit_group)
        p_box.append(notes_group)
        p_box.append(props_group)
        p_box.append(self.tags)
        p_box.append(misc_group)
        # Box
        box = Gtk.Box(orientation="vertical")
        box.append(self.status)
        box.append(p_box)
        # Toolbar View
        toolbar_view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(
                child=Adw.Clamp(
                    child=box,
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
        delete_btn.connect("clicked", self.on_tag_deleted, tag)
        tag.add_suffix(delete_btn)
        self.tags.add(tag)

    def update_info(self, parent):
        Log.debug("Details: Update info")

        self.parent = parent
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
        self.save_btn.set_sensitive(False)
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

    def on_save_btn_clicked(self, btn):
        Log.info("Details: Save")
        # Set text
        text = self.edit_entry.props.text
        if text.strip(" \n\t") != "":
            self.parent.update_props(["text"], [text])
            self.parent.task_row.set_title(Markup.find_url(Markup.escape(text)))
        else:
            self.edit_entry.set_text(self.parent.get_prop("text"))
        # Set completion
        pc = self.percent_complete.get_value()
        self.parent.completed_btn.set_active(pc == 100)
        self.parent.update_props(["percent_complete"], [pc])
        self.percent_complete.set_value(pc)
        # Set tags
        tag_arr: list[str] = []
        for i, row in enumerate(get_children(self.tag_entry.get_parent())):
            # Skip entry
            if i == 0:
                continue
            tag_arr.append(row.get_title())
        # Set new props
        self.parent.update_props(
            [
                "start_date",
                "end_date",
                "notes",
                "percent_complete",
                "priority",
                "tags",
                "synced",
            ],
            [
                self.start_datetime.get_datetime(),
                self.end_datetime.get_datetime(),
                self.notes.props.text,
                int(self.percent_complete.get_value()),
                int(self.priority.get_value()),
                ",".join(tag_arr),
                False,
            ],
        )
        self.save_btn.set_sensitive(False)
        # Sync
        Sync.sync()

    def on_datetime_changed(self, dt):
        pass

    def on_start_time_changed(self, *args):
        Log.debug("Details: change start time")
        self.start_datetime_row.set_title(self.start_datetime.get_human_datetime())
        self.save_btn.set_sensitive(True)

    def on_end_time_changed(self, *args):
        Log.debug("Details: change end time")
        self.end_datetime_row.set_title(self.end_datetime.get_human_datetime())
        self.save_btn.set_sensitive(True)

    def on_copy_text_clicked(self, _btn):
        Log.info("Details: Copy to clipboard")
        Gdk.Display.get_default().get_clipboard().set(self.parent.get_prop("text"))
        self.window.add_toast(_("Copied to Clipboard"))  # pyright:ignore

    def on_delete_btn_clicked(self, _btn):
        self.parent.delete()
        self.status.set_visible(True)

    def on_export(self, _btn):
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
                datetime.fromisoformat(task["start_date"])
                if task["start_date"]
                else datetime.now(),
            )
            if task["end_date"]:
                event.add("dtend", datetime.fromisoformat(task["end_date"]))
            calendar.add_component(event)

            with open(file.get_path(), "wb") as f:
                f.write(calendar.to_ical())
            self.window.add_toast(_("Exported"))  # type:ignore

        dialog = Gtk.FileDialog(initial_name=f"{self.parent.uid}.ics")
        dialog.save(self.window, None, _confirm)

    def on_style_selected(self, btn: Gtk.Button, color: str) -> None:
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

    def on_tag_added(self, entry: Adw.EntryRow) -> None:
        text = entry.get_text().strip(" \n\t")
        if text == "":
            return
        Log.debug("Add tag")
        self.add_tag(entry.get_text())
        entry.set_text("")
        self.save_btn.set_sensitive(True)

    def on_tag_deleted(self, btn, tag):
        Log.debug("Remove tag")
        self.tags.remove(tag)
        self.save_btn.set_sensitive(True)
