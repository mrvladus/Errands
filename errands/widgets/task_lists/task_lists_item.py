# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.task_lists.task_lists import TaskLists
    from errands.widgets.task_list.task_list import TaskList
    from errands.widgets.task import Task
    from errands.widgets.window import Window

from datetime import datetime
from caldav import Calendar, Todo
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.widgets.components import Box
from errands.widgets.task import Task
from gi.repository import Adw, Gtk, Gio, GObject, Gdk


class TaskListsItem(Gtk.ListBoxRow):
    def __init__(
        self, task_list: TaskList, list_box, lists: TaskLists, window: Window
    ) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.uid: str = task_list.list_uid
        self.window: Window = window
        self.list_box: Gtk.ListBox = list_box
        self.lists: TaskLists = lists
        self._build_ui()
        self._add_actions()

    def _add_actions(self) -> None:
        group = Gio.SimpleActionGroup()
        self.insert_action_group(name="list_item", group=group)

        def _create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        def _delete(*args):
            def _confirm(_, res):
                if res == "cancel":
                    Log.debug("ListItem: Deleting list is cancelled")
                    return

                Log.info(f"Lists: Delete list '{self.uid}'")
                UserData.run_sql(
                    f"UPDATE lists SET deleted = 1 WHERE uid = '{self.uid}'",
                    f"DELETE FROM tasks WHERE list_uid = '{self.uid}'",
                )
                self.window.stack.remove(self.task_list)
                # Switch row
                next_row = self.get_next_sibling()
                prev_row = self.get_prev_sibling()
                self.list_box.remove(self)
                if next_row or prev_row:
                    self.list_box.select_row(next_row or prev_row)
                else:
                    self.window.stack.set_visible_child_name("status")
                    self.lists.status_page.set_visible(True)

                # Remove items from trash
                for item in get_children(self.window.trash.trash_list):
                    if item.task_widget.list_uid == self.uid:
                        self.window.trash.trash_list.remove(item)

                Sync.sync()

            dialog = Adw.MessageDialog(
                transient_for=self.window,
                hide_on_close=True,
                heading=_("Are you sure?"),
                body=_("List will be permanently deleted"),
                default_response="delete",
                close_response="cancel",
            )
            dialog.add_response("cancel", _("Cancel"))
            dialog.add_response("delete", _("Delete"))
            dialog.set_response_appearance("delete", Adw.ResponseAppearance.DESTRUCTIVE)
            dialog.connect("response", _confirm)
            dialog.present()

        def _rename(*args):
            def _entry_activated(_, dialog):
                if dialog.get_response_enabled("save"):
                    dialog.response("save")
                    dialog.close()

            def _entry_changed(entry, _, dialog):
                text = entry.props.text.strip(" \n\t")
                names = [i["name"] for i in UserData.get_lists_as_dicts()]
                dialog.set_response_enabled("save", text and text not in names)

            def _confirm(_, res, entry):
                if res == "cancel":
                    Log.debug("ListItem: Editing list name is cancelled")
                    return
                Log.info(f"ListItem: Rename list {self.uid}")

                text = entry.props.text.rstrip().lstrip()
                UserData.run_sql(
                    (
                        "UPDATE lists SET name = ?, synced = 0 WHERE uid = ?",
                        (text, self.uid),
                    )
                )
                self.task_list.title.set_title(text)
                Sync.sync()

            entry = Gtk.Entry(placeholder_text=_("New Name"))
            entry.get_buffer().props.text = self.label.get_label()
            dialog = Adw.MessageDialog(
                transient_for=self.window,
                hide_on_close=True,
                heading=_("Rename List"),
                default_response="save",
                close_response="cancel",
                extra_child=entry,
            )
            dialog.add_response("cancel", _("Cancel"))
            dialog.add_response("save", _("Save"))
            dialog.set_response_enabled("save", False)
            dialog.set_response_appearance("save", Adw.ResponseAppearance.SUGGESTED)
            dialog.connect("response", _confirm, entry)
            entry.connect("activate", _entry_activated, dialog)
            entry.connect("notify::text", _entry_changed, dialog)
            dialog.present()

        def _export(*args):
            def _confirm(dialog, res):
                try:
                    file = dialog.save_finish(res)
                except:
                    Log.debug("List: Export cancelled")
                    return

                Log.info(f"List: Export '{self.uid}'")

                tasks = UserData.get_tasks_as_dicts(self.uid)
                calendar = Calendar()
                calendar.add("x-wr-calname", self.label.get_label())
                for task in tasks:
                    event = Todo()
                    event.add("uid", task["uid"])
                    event.add("related-to", task["parent"])
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
                        event.add(
                            "due",
                            datetime.fromisoformat(task["end_date"])
                            if task["end_date"]
                            else datetime.now(),
                        )
                    calendar.add_component(event)

                try:
                    with open(file.get_path(), "wb") as f:
                        f.write(calendar.to_ical())
                except Exception as e:
                    Log.error(f"List: Export failed. {e}")
                    self.window.add_toast(_("Export failed"))

                self.window.add_toast(_("Exported"))

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(
                initial_name=f"{self.uid}.ics", default_filter=filter
            )
            dialog.save(self.window, None, _confirm)

        _create_action("delete", _delete)
        _create_action("rename", _rename)
        _create_action("export", _export)

    def _build_ui(self) -> None:
        self.add_css_class("task-lists-item")
        # Label
        self.label = Gtk.Label(
            halign="start",
            hexpand=True,
            ellipsize=3,
        )
        self.task_list.title.bind_property(
            "title",
            self.label,
            "label",
            GObject.BindingFlags.SYNC_CREATE,
        )
        # Menu
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Rename"), "list_item.rename")
        menu.append(_("Delete"), "list_item.delete")
        menu.append(_("Export"), "list_item.export")
        # Click controller
        ctrl = Gtk.GestureClick()
        ctrl.connect("released", self._on_click)
        self.add_controller(ctrl)
        # Drop controller
        drop_ctrl = Gtk.DropTarget.new(actions=Gdk.DragAction.MOVE, type=Task)
        drop_ctrl.connect("drop", self._on_task_drop)
        self.add_controller(drop_ctrl)
        self.set_child(
            Box(
                children=[
                    self.label,
                    Gtk.MenuButton(
                        menu_model=menu,
                        icon_name="view-more-symbolic",
                        tooltip_text=_("Menu"),
                    ),
                ],
                css_classes=["toolbar"],
            )
        )

    def _on_click(self, *args) -> None:
        self.window.stack.set_visible_child_name(self.label.get_label())
        self.window.split_view.set_show_content(True)

    def _on_task_drop(self, _drop, task: Task, _x, _y):
        if task.list_uid == self.uid:
            return
        task.update_props(["list_uid", "parent"], [self.uid, ""])
        Sync.sync()
