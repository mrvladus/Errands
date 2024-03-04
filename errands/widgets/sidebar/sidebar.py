# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
import os
from typing import TYPE_CHECKING
from errands.widgets.sidebar.today_item import TodayItem
from errands.widgets.sidebar.trash_item import TrashItem

from errands.widgets.trash import Trash

if TYPE_CHECKING:
    from errands.widgets.window import Window
    from errands.lib.plugins import PluginBase

import time
from datetime import datetime
from icalendar import Calendar, Todo
from errands.lib.plugins import PluginsLoader
from errands.lib.data import TaskListData, UserData
from errands.lib.utils import get_children
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.widgets.components import Box, ConfirmDialog
from errands.widgets.task import Task
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk, Gio, GObject, Gdk, GLib  # type:ignore


# class SidebarPluginsList(Adw.Bin):
#     def __init__(self, sidebar: Sidebar):
#         super().__init__()
#         self.sidebar: Sidebar = sidebar
#         self._build_ui()
#         self.load_plugins()

#     def _build_ui(self) -> None:
#         self.plugins_list: Gtk.ListBox = Gtk.ListBox(css_classes=["navigation-sidebar"])
#         self.plugins_list.connect("row-selected", self._on_row_selected)
#         self.set_child(
#             Box(
#                 children=[SidebarListTitle(_("Plugins")), self.plugins_list],
#                 orientation="vertical",
#             )
#         )

#     def add_plugin(self, plugin_row: SidebarPluginListItem):
#         self.plugins_list.append(plugin_row)

#     def get_plugins(self) -> list[Gtk.ListBoxRow]:
#         return get_children(self.plugins_list)

#     def load_plugins(self):
#         plugin_loader: PluginsLoader = (
#             self.sidebar.window.get_application().plugins_loader
#         )
#         if not plugin_loader or not plugin_loader.plugins:
#             self.set_visible(False)
#             return
#         for plugin in plugin_loader.plugins:
#             self.add_plugin(SidebarPluginListItem(plugin, self.sidebar))

#     def _on_row_selected(self, _, row: Gtk.ListBoxRow):
#         if row:
#             row.activate()


# class SidebarPluginListItem(Gtk.ListBoxRow):
#     def __init__(self, plugin: PluginBase, sidebar: Sidebar):
#         super().__init__()
#         self.name = plugin.name
#         self.icon = plugin.icon
#         self.main_view = plugin.main_view
#         self.description = plugin.description
#         self.sidebar = sidebar
#         self._build_ui()

#     def _build_ui(self) -> None:
#         self.set_child(
#             Box(
#                 children=[
#                     Gtk.Image(icon_name=self.icon),
#                     Gtk.Label(label=self.name, halign=Gtk.Align.START),
#                 ],
#                 css_classes=["toolbar"],
#             )
#         )
#         ctrl: Gtk.GestureClick = Gtk.GestureClick()
#         ctrl.connect("released", self.do_activate)
#         self.add_controller(ctrl)

#         self.sidebar.window.stack.add_titled(self.main_view, self.name, self.name)

#     def do_activate(self, *args) -> None:
#         self.sidebar.window.stack.set_visible_child_name(self.name)
#         self.sidebar.task_lists.lists.unselect_all()


@Gtk.Template(filename=f"{os.path.dirname(__file__)}/sidebar.ui")
class Sidebar(Adw.Bin):
    __gtype_name__ = "Sidebar"

    sync_indicator: Gtk.Spinner = Gtk.Template.Child()
    add_list_btn: Gtk.Button = Gtk.Template.Child()
    list_box: Gtk.ListBox = Gtk.Template.Child()
    status_page: Adw.StatusPage = Gtk.Template.Child()

    def __init__(self):
        super().__init__()
        self.window: Window = Adw.Application.get_default().get_active_window()
        self.__build_ui()
        self.update_ui()

    def __build_ui(self) -> None:
        # --- Categories --- #

        # Today
        self.today_item = TodayItem()
        self.list_box.append(self.today_item)

        # Trash
        self.trash_item = TrashItem()
        self.list_box.append(self.trash_item)

        # --- Separator --- #
        separator = Gtk.Box(spacing=6, css_classes=["dim-label"])
        separator.append(Gtk.Separator(valign=Gtk.Align.CENTER, hexpand=True))
        separator.append(
            Gtk.Label(label=_("Task Lists"), css_classes=["caption-heading"])
        )
        separator.append(Gtk.Separator(valign=Gtk.Align.CENTER, hexpand=True))
        self.list_box.append(
            Gtk.ListBoxRow(
                selectable=False,
                activatable=False,
                child=separator,
            )
        )

    def add_task_list(self, list_dict: TaskListData) -> SidebarTaskListItem:
        Log.debug(f"Sidebar: Add Task List '{list_dict['uid']}'")
        row: SidebarTaskListItem = SidebarTaskListItem(list_dict, self)
        self.list_box.append(row)
        return row

    @property
    def rows(self) -> list[Gtk.ListBoxRow]:
        return get_children(self.list_box)

    @property
    def task_lists_rows(self) -> list[SidebarTaskListItem]:
        task_lists: list[SidebarTaskListItem] = []
        for row in get_children(self.list_box):
            if isinstance(row, SidebarTaskListItem):
                task_lists.append(row)
        return task_lists

    @property
    def task_lists(self) -> list[TaskList]:
        return [l.task_list for l in self.task_lists_rows]

    def update_ui(self) -> None:
        Log.debug("Sidebar: Update UI")

        lists: list[TaskListData] = UserData.get_lists_as_dicts()
        uids: list[str] = [l["uid"] for l in lists]

        # Delete lists
        for l in self.task_lists_rows:
            if l.uid not in uids:
                Log.debug(f"Sidebar: Delete list {l.uid}")
                self.list_box.select_row(l.get_prev_sibling())
                self.window.stack.remove(l.task_list)
                self.list_box.remove(l)

        # Add lists
        lists_uids = [l.list_uid for l in self.task_lists]
        for l in lists:
            if l["uid"] not in lists_uids:
                self.add_task_list(l)

        # Update rows
        for row in self.rows:
            if hasattr(row, "update_ui"):
                row.update_ui()

        # Select last list
        for row in self.rows:
            if hasattr(row, "name") and row.name == GSettings.get("last-open-list"):
                Log.debug("Sidebar: Select last opened item")
                self.list_box.select_row(row)
                break

        # Show status
        length: int = len(self.task_lists)
        self.status_page.set_visible(length == 0)
        if length == 0:
            self.window.stack.set_visible_child_name("status")

    @Gtk.Template.Callback()
    def _on_add_btn_clicked(self, _btn):
        def _entry_activated(_, dialog):
            if dialog.get_response_enabled("add"):
                dialog.response("add")
                dialog.close()

        def _entry_changed(entry: Gtk.Entry, _, dialog):
            text = entry.props.text.strip(" \n\t")
            names = [i["name"] for i in UserData.get_lists_as_dicts()]
            dialog.set_response_enabled("add", text and text not in names)

        def _confirm(_, res, entry: Gtk.Entry):
            if res == "cancel":
                return

            name = entry.props.text.rstrip().lstrip()
            list_dict = UserData.add_list(name)
            row = self.add_task_list(list_dict)
            row.activate()
            Sync.sync()

        entry = Gtk.Entry(placeholder_text=_("New List Name"))
        dialog = Adw.MessageDialog(
            transient_for=self.window,
            hide_on_close=True,
            heading=_("Add List"),
            default_response="add",
            close_response="cancel",
            extra_child=entry,
        )
        dialog.add_response("cancel", _("Cancel"))
        dialog.add_response("add", _("Add"))
        dialog.set_response_enabled("add", False)
        dialog.set_response_appearance("add", Adw.ResponseAppearance.SUGGESTED)
        dialog.connect("response", _confirm, entry)
        entry.connect("activate", _entry_activated, dialog)
        entry.connect("notify::text", _entry_changed, dialog)
        dialog.present()

    @Gtk.Template.Callback()
    def _on_row_selected(self, _, row: Gtk.ListBoxRow):
        if row:
            row.activate()


class SidebarTaskListItem(Adw.ActionRow):
    def __init__(self, list_dict: TaskListData, sidebar: Sidebar) -> None:
        super().__init__()
        self.window: Window = sidebar.window
        self.sidebar: Sidebar = sidebar
        self.list_box: Gtk.ListBox = sidebar.list_box
        self.uid: str = list_dict["uid"]
        self.name: str = list_dict["name"]
        self.__build_ui()
        self.__add_actions()

    def __add_actions(self) -> None:
        group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="list_item", group=group)

        def _create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        def _delete(*args):
            def __confirm(_, res):
                if res == "cancel":
                    Log.debug("ListItem: Deleting list is cancelled")
                    return

                Log.info(f"Lists: Delete list '{self.uid}'")
                UserData.run_sql(
                    f"UPDATE lists SET deleted = 1 WHERE uid = '{self.uid}'",
                    f"DELETE FROM tasks WHERE list_uid = '{self.uid}'",
                )
                self.sidebar.update_ui()
                Sync.sync()

            ConfirmDialog(
                _("List will be permanently deleted"),
                _("Delete"),
                Adw.ResponseAppearance.DESTRUCTIVE,
                __confirm,
            )

        def _rename(*args):
            def _entry_activated(_, dialog: Adw.MessageDialog):
                if dialog.get_response_enabled("save"):
                    dialog.response("save")
                    dialog.close()

            def _entry_changed(entry: Gtk.Entry, _, dialog: Adw.MessageDialog):
                text = entry.props.text.strip(" \n\t")
                names = [i["name"] for i in UserData.get_lists_as_dicts()]
                dialog.set_response_enabled("save", text and text not in names)

            def _confirm(_, res, entry: Gtk.Entry):
                if res == "cancel":
                    Log.debug("ListItem: Editing list name is cancelled")
                    return
                Log.info(f"ListItem: Rename list {self.uid}")

                text: str = entry.props.text.rstrip().lstrip()
                UserData.run_sql(
                    (
                        "UPDATE lists SET name = ?, synced = 0 WHERE uid = ?",
                        (text, self.uid),
                    )
                )
                self.update_ui()
                self.sidebar.trash_item.update_ui()
                self.sidebar.today_item.update_ui()
                Sync.sync()

            entry: Gtk.Entry = Gtk.Entry(placeholder_text=_("New Name"))
            entry.get_buffer().props.text = self.label.get_label()
            dialog: Adw.MessageDialog = Adw.MessageDialog(
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

                tasks: list[dict] = UserData.get_tasks_as_dicts(self.uid)
                calendar: Calendar = Calendar()
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
                        (
                            datetime.fromisoformat(task["start_date"])
                            if task["start_date"]
                            else datetime.now()
                        ),
                    )
                    if task["end_date"]:
                        event.add(
                            "due",
                            (
                                datetime.fromisoformat(task["end_date"])
                                if task["end_date"]
                                else datetime.now()
                            ),
                        )
                    calendar.add_component(event)

                try:
                    with open(file.get_path(), "wb") as f:
                        f.write(calendar.to_ical())
                except Exception as e:
                    Log.error(f"List: Export failed. {e}")
                    self.window.add_toast(_("Export failed"))

                self.window.add_toast(_("Exported"))

            filter: Gtk.FileFilter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog: Gtk.FileDialog = Gtk.FileDialog(
                initial_name=f"{self.uid}.ics", default_filter=filter
            )
            dialog.save(self.window, None, _confirm)

        _create_action("delete", _delete)
        _create_action("rename", _rename)
        _create_action("export", _export)

    def __build_ui(self) -> None:

        # Add Task List page
        self.task_list: TaskList = TaskList(self.window, self.uid)
        self.stack_page: Adw.ViewStackPage = self.window.stack.add_titled(
            child=self.task_list, name=self.name, title=self.name
        )

        # Customize internal AdwActionRow styles
        internal_box: Gtk.Box = self.get_child()
        internal_box.remove_css_class("header")
        internal_box.set_margin_start(6)
        internal_box.set_spacing(12)

        self.props.height_request = 50
        self.set_title(self.name)
        self.set_activatable(True)
        self.add_css_class("sidebar-item")
        self.connect("activated", self.__activated)

        # Counter
        self.__counter = Gtk.Label(css_classes=["dim-label", "caption"])

        # Menu
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Rename"), "list_item.rename")
        menu.append(_("Delete"), "list_item.delete")
        menu.append(_("Export"), "list_item.export")

        # Drop controller
        drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
            actions=Gdk.DragAction.MOVE, type=Task
        )
        drop_ctrl.connect("drop", self._on_task_drop)
        self.add_controller(drop_ctrl)

        # Drop hover controller
        drop_hover_ctrl: Gtk.DropControllerMotion = Gtk.DropControllerMotion()
        drop_hover_ctrl.connect("enter", self._on_drop_hover)
        self.add_controller(drop_hover_ctrl)

        # Suffix
        suffix_box: Gtk.Box = Gtk.Box(spacing=12)
        suffix_box.append(self.__counter)
        suffix_box.append(
            Gtk.MenuButton(
                menu_model=menu,
                icon_name="view-more-symbolic",
                tooltip_text=_("Menu"),
                css_classes=["flat"],
                valign=Gtk.Align.CENTER,
            )
        )
        self.add_suffix(suffix_box)

    def _on_drop_hover(self, ctrl: Gtk.DropControllerMotion, _x, _y):
        """
        Switch list on dnd hover after DELAY_SECONDS
        """

        DELAY_SECONDS: float = 0.7
        entered_at: float = time.time()

        def _switch_delay():
            if ctrl.contains_pointer():
                if time.time() - entered_at >= DELAY_SECONDS:
                    self.activate()
                    return False
                else:
                    return True
            else:
                return False

        GLib.timeout_add(100, _switch_delay)

    def _on_task_drop(self, _drop, task: Task, _x, _y):
        """
        Move task and sub-tasks to new list
        """

        if task.list_uid == self.uid:
            return

        Log.info(f"Lists: Move '{task.uid}' to '{self.uid}' list")
        UserData.move_task_to_list(task.uid, task.list_uid, self.uid, "", False)
        # uid: str = task.uid
        # task.purge()
        # self.task_list.add_task(uid)
        Sync.sync()

    def __activated(self, *args) -> None:
        Log.debug(f"Sidebar: Switch to list '{self.uid}'")

        self.window.stack.set_visible_child_name(self.get_title())
        self.window.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", self.name)

    def update_ui(self):
        Log.debug(f"Sidebar: List Item: Update UI '{self.uid}'")

        # Update title
        self.name = [
            i["name"] for i in UserData.get_lists_as_dicts() if i["uid"] == self.uid
        ][0]
        self.set_title(self.name)
        self.stack_page.set_name(self.name)
        self.stack_page.set_title(self.name)

        # Update counter
        size: int = len(
            [
                t
                for t in UserData.get_tasks_as_dicts(self.uid)
                if not t["trash"] and not t["deleted"]
            ]
        )
        self.__counter.set_label(str(size) if size > 0 else "")

        # Update list
        self.task_list.update_ui()
