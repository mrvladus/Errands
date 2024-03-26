# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


import os
from datetime import datetime

from gi.repository import Adw, Gdk, Gio, GObject, Gtk, GtkSource  # type:ignore
from icalendar import Calendar, Event

from errands.lib.data import UserData
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.lib.utils import get_children
from errands.widgets import task
from errands.widgets.components.datetime_picker import DateTimePicker
from errands.widgets.task.tag import Tag
from errands.widgets.task.task import Task
from errands.widgets.task.toolbar.tags_list_item import TagsListItem


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TaskToolbar(Gtk.Revealer):
    __gtype_name__ = "TaskToolbar"

    task: Task = GObject.Property(type=Task)

    notes_btn: Gtk.MenuButton = Gtk.Template.Child()
    notes_buffer: GtkSource.Buffer = Gtk.Template.Child()
    priority_btn: Gtk.MenuButton = Gtk.Template.Child()
    created_label: Gtk.Label = Gtk.Template.Child()
    changed_label: Gtk.Label = Gtk.Template.Child()
    start_date_time: DateTimePicker = Gtk.Template.Child()
    due_date_time: DateTimePicker = Gtk.Template.Child()
    date_time_btn: Gtk.MenuButton = Gtk.Template.Child()
    date_stack: Adw.ViewStack = Gtk.Template.Child()
    tags_list: Gtk.ListBox = Gtk.Template.Child()
    priority: Gtk.SpinButton = Gtk.Template.Child()

    can_sync = True

    def __init__(self) -> None:
        super().__init__()
        self.__add_actions()
        # Set notes theme
        Adw.StyleManager.get_default().bind_property(
            "dark",
            self.notes_buffer,
            "style-scheme",
            GObject.BindingFlags.SYNC_CREATE,
            lambda _, is_dark: self.notes_buffer.set_style_scheme(
                GtkSource.StyleSchemeManager.get_default().get_scheme(
                    "Adwaita-dark" if is_dark else "Adwaita"
                )
            ),
        )
        lm: GtkSource.LanguageManager = GtkSource.LanguageManager.get_default()
        self.notes_buffer.set_language(lm.get_language("markdown"))

    def __add_actions(self) -> None:
        self.group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="task_toolbar", group=self.group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction(name=name)
            action.connect("activate", callback)
            self.group.add_action(action)

        def __edit(*args):
            self.task.entry_row.set_text(self.task.get_prop("text"))
            self.task.entry_row.set_visible(True)

        def __export(*args):
            def __confirm(dialog, res):
                try:
                    file = dialog.save_finish(res)
                except:
                    Log.debug("List: Export cancelled")
                    return

                Log.info(f"Task: Export '{self.uid}'")

                task = [
                    i
                    for i in UserData.get_tasks_as_dicts(self.task.list_uid)
                    if i.uid == self.task.uid
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
                self.task.window.add_toast(_("Exported"))

            dialog = Gtk.FileDialog(initial_name=f"{self.task.uid}.ics")
            dialog.save(self.task.window, None, __confirm)

        def __copy_to_clipboard(*args):
            Log.info("Task: Copy text to clipboard")
            Gdk.Display.get_default().get_clipboard().set(self.task.get_prop("text"))
            self.task.window.add_toast(_("Copied to Clipboard"))

        __create_action("edit", __edit)
        __create_action("copy_to_clipboard", __copy_to_clipboard)
        __create_action("export", __export)
        __create_action("move_to_trash", lambda *_: self.task.delete())

    def update_ui(self) -> None:
        # Show toolbar
        self.set_reveal_child(self.task.get_prop("toolbar_shown"))

        # Update Date and Time
        self.due_date_time.datetime = self.task.get_prop("due_date")
        self.date_time_btn.get_child().props.label = (
            f"{self.due_date_time.human_datetime}"
        )

        # Update notes button css
        if self.task.get_prop("notes"):
            self.notes_btn.add_css_class("accent")
        else:
            self.notes_btn.remove_css_class("accent")

        # Update priority button css
        priority: int = self.task.get_prop("priority")
        self.priority_btn.props.css_classes = ["flat"]
        if 0 < priority < 5:
            self.priority_btn.add_css_class("error")
        elif 4 < priority < 9:
            self.priority_btn.add_css_class("warning")
        elif priority == 9:
            self.priority_btn.add_css_class("accent")

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_date_time_toggled(self, _btn: Gtk.MenuButton, active: bool) -> None:
        self.start_date_time.datetime = self.task.get_prop("start_date")
        self.due_date_time.datetime = self.task.get_prop("due_date")

    @Gtk.Template.Callback()
    def _on_date_time_start_set(self, *args) -> None:
        self.task.update_props(["start_date"], [self.start_date_time.datetime])

    @Gtk.Template.Callback()
    def _on_date_time_due_set(self, *args) -> None:
        self.task.update_props(["due_date"], [self.due_date_time.datetime])
        self.update_ui()

    @Gtk.Template.Callback()
    def _on_menu_toggled(self, _btn: Gtk.MenuButton, active: bool):
        if not active:
            return
        created_date: str = datetime.fromisoformat(
            self.task.get_prop("created_at")
        ).strftime("%Y.%m.%d %H:%M:%S")
        changed_date: str = datetime.fromisoformat(
            self.task.get_prop("changed_at")
        ).strftime("%Y.%m.%d %H:%M:%S")
        self.created_label.set_label(_("Created:") + " " + created_date)
        self.changed_label.set_label(_("Changed:") + " " + changed_date)

    @Gtk.Template.Callback()
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
                self.tags_list.append(TagsListItem(tag, self.task))

        # Toggle tags
        task_tags: list[str] = [t.title for t in self.task.tags]
        tags_items: list[TagsListItem] = get_children(self.tags_list)
        for t in tags_items:
            t.block_signals = True
            t.toggle_btn.set_active(t.title in task_tags)
            t.block_signals = False

        self.tags_list.set_visible(len(get_children(self.tags_list)) > 0)

    @Gtk.Template.Callback()
    def _on_notes_toggled(self, btn: Gtk.MenuButton, *_):
        notes: str = self.task.get_prop("notes")
        if btn.get_active():
            self.notes_buffer.set_text(notes)
        else:
            text: str = self.notes_buffer.props.text
            if text == notes:
                return
            Log.info("Task: Change notes")
            self.task.update_props(["notes", "synced"], [text, False])
            self.update_ui()
            # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_priority_toggled(self, btn: Gtk.MenuButton, *_):
        priority: int = self.task.get_prop("priority")
        if btn.get_active():
            self.priority.set_value(priority)
        else:
            new_priority: int = self.priority.get_value_as_int()
            if priority != new_priority:
                Log.debug(f"Task Toolbar: Set priority to '{new_priority}'")
                self.task.update_props(["priority", "synced"], [new_priority, False])
                self.update_ui()
                # Sync.sync(False)

    @Gtk.Template.Callback()
    def _on_priority_selected(self, box: Gtk.ListBox, row: Gtk.ListBoxRow):
        rows: list[Gtk.ListBoxRow] = get_children(box)
        for i, r in enumerate(rows):
            if r == row:
                index = i
                break
        match index:
            case 0:
                self.priority.set_value(1)
            case 1:
                self.priority.set_value(5)
            case 2:
                self.priority.set_value(9)
            case 3:
                self.priority.set_value(0)
        self.priority_btn.popdown()

    @Gtk.Template.Callback()
    def _on_accent_color_btn_toggled(self, btn: Gtk.MenuButton, *_):
        if btn.get_active():
            color: str = self.task.get_prop("color")
            color_btns = get_children(
                btn.get_popover().get_child().get_first_child()
            ) + get_children(btn.get_popover().get_child().get_last_child())
            for btn in color_btns:
                btn_color = btn.get_buildable_id()
                if btn_color == color:
                    self.can_sync = False
                    btn.set_active(True)
                    self.can_sync = True

    @Gtk.Template.Callback()
    def _on_accent_color_selected(self, btn: Gtk.CheckButton):
        if not btn.get_active() or not self.can_sync:
            return
        color: str = btn.get_buildable_id()
        Log.debug(f"Task: change color to '{color}'")
        if color != self.task.get_prop("color"):
            self.task.update_props(
                ["color", "synced"], [color if color != "none" else "", False]
            )
            self.task.update_ui(False)
            # Sync.sync(False)
