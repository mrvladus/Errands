# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

from errands.widgets.trash import Trash

if TYPE_CHECKING:
    from errands.widgets.window import Window
    from errands.lib.plugins import PluginBase

import time
from datetime import datetime
from icalendar import Calendar, Todo
from errands.lib.plugins import PluginsLoader
from errands.lib.data import UserData
from errands.lib.utils import get_children
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.widgets.components import Box
from errands.widgets.task import Task
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk, Gio, GObject, Gdk, GLib  # type:ignore


class SidebarHeaderBar(Adw.Bin):
    def __init__(self, list_box: Gtk.ListBox, window: Window):
        super().__init__()
        self.list_box = list_box
        self.window: Window = window
        self._build_ui()

    def _build_ui(self):
        # HeaderBar
        hb: Adw.HeaderBar = Adw.HeaderBar(
            title_widget=Gtk.Label(
                label=_("Errands"),
                css_classes=["heading"],
            )
        )
        self.set_child(hb)

        # Import menu
        import_menu: Gio.Menu = Gio.Menu.new()
        import_menu.append(_("Import List"), "app.import")

        # Add list button
        self.add_list_btn: Adw.SplitButton = Adw.SplitButton(
            icon_name="list-add-symbolic",
            tooltip_text=_("Add List (Ctrl+A)"),
            menu_model=import_menu,
            dropdown_tooltip=_("More Options"),
        )
        ctrl = Gtk.ShortcutController(scope=1)
        ctrl.add_shortcut(
            Gtk.Shortcut(
                trigger=Gtk.ShortcutTrigger.parse_string("<Primary>A"),
                action=Gtk.ShortcutAction.parse_string("activate"),
            )
        )
        self.add_list_btn.add_controller(ctrl)
        self.add_list_btn.connect("clicked", self._on_add_btn_clicked)
        hb.pack_start(self.add_list_btn)

        # Main menu
        menu: Gio.Menu = Gio.Menu.new()
        self.top_section: Gio.Menu = Gio.Menu.new()
        self.top_section.append(_("Sync / Fetch Tasks"), "app.sync")
        menu.append_section(None, self.top_section)
        bottom_section: Gio.Menu = Gio.Menu.new()
        bottom_section.append(_("Preferences"), "app.preferences")
        bottom_section.append(_("Keyboard Shortcuts"), "win.show-help-overlay")
        bottom_section.append(_("About Errands"), "app.about")
        menu.append_section(None, bottom_section)
        self.menu_btn: Gtk.MenuButton = Gtk.MenuButton(
            menu_model=menu,
            primary=True,
            icon_name="open-menu-symbolic",
            tooltip_text=_("Main Menu"),
        )
        hb.pack_end(self.menu_btn)

        # Sync indicator
        self.sync_indicator: Gtk.Spinner = Gtk.Spinner(
            tooltip_text=_("Syncing..."), visible=False, spinning=True
        )
        hb.pack_end(self.sync_indicator)

    def _on_add_btn_clicked(self, btn) -> None:
        def _entry_activated(_, dialog):
            if dialog.get_response_enabled("add"):
                dialog.response("add")
                dialog.close()

        def _entry_changed(entry, _, dialog):
            text = entry.props.text.strip(" \n\t")
            names = [i["name"] for i in UserData.get_lists_as_dicts()]
            dialog.set_response_enabled("add", text and text not in names)

        def _confirm(_, res, entry):
            if res == "cancel":
                return

            name = entry.props.text.rstrip().lstrip()
            uid = UserData.add_list(name)
            row = self.list_box.add_list(name, uid)
            self.list_box.select_row(row)
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


class Sidebar(Adw.Bin):
    def __init__(self, window: Window):
        super().__init__()
        self.window = window
        self.__build_ui()

    def __build_ui(self) -> None:
        # --- Status Page --- #
        self.status_page: Adw.StatusPage = Adw.StatusPage(
            title=_("Add new List"),
            description=_('Click "+" button'),
            icon_name="errands-lists-symbolic",
            css_classes=["compact"],
            vexpand=True,
        )

        # --- List Box --- #

        self.list_box = Gtk.ListBox(
            css_classes=["navigation-sidebar"], activate_on_single_click=False
        )
        self.list_box.connect("row-selected", self.__on_row_changed)

        # --- Categories --- #

        self.list_box.append(SidebarTodayItem(self.window))
        self.list_box.append(SidebarTrashItem(self.window))

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

        # --- Task Lists --- #

        self.__load_task_lists()

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        vbox.append(self.list_box)
        vbox.append(self.status_page)

        # Toolbar view
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(content=vbox)
        toolbar_view.add_top_bar(SidebarHeaderBar(self.list_box, self.window))

        self.set_child(toolbar_view)

    def add_task_list(self, name: str, uid: str) -> SidebarTaskListItem:
        task_list: TaskList = TaskList(self.window, uid, self)
        page: Adw.ViewStackPage = self.window.stack.add_titled(
            child=task_list, name=name, title=name
        )
        task_list.headerbar.title.bind_property(
            "title", page, "title", GObject.BindingFlags.SYNC_CREATE
        )
        row: SidebarTaskListItem = SidebarTaskListItem(task_list, self.list_box)
        self.list_box.append(row)
        return row

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

    def __load_task_lists(self):
        lists: list[dict] = [
            i for i in UserData.get_lists_as_dicts() if not i["deleted"]
        ]
        for l in lists:
            row: SidebarTaskListItem = self.add_task_list(l["name"], l["uid"])
            if GSettings.get("last-open-list") == l["name"]:
                self.list_box.select_row(row)
        self.status_page.set_visible(len(lists) == 0)

    def __on_row_changed(self, _, row: Gtk.ListBoxRow):
        if isinstance(row, SidebarTaskListItem):
            name = row.label.get_label()
            self.window.stack.set_visible_child_name(name)
            self.window.split_view.set_show_content(True)
            GSettings.set("last-open-list", "s", name)
        elif isinstance(row, SidebarTrashItem):
            self.window.stack.set_visible_child_name("errands_trash_page")
            self.window.split_view.set_show_content(True)
        elif isinstance(row, SidebarTodayItem):
            self.window.stack.set_visible_child_name("errands_today_page")
            self.window.split_view.set_show_content(True)

    def update_ui(self) -> None:
        Log.debug("Sidebar: Update UI")

        # self.sidebar.plugins_list.plugins_list.unselect_all()

        # # Delete lists
        # uids: list[str] = [i["uid"] for i in UserData.get_lists_as_dicts()]
        # for row in self._get_task_lists_items():
        #     if row.uid not in uids:
        #         prev_child: SidebarTaskListsItem | None = row.get_prev_sibling()
        #         next_child: SidebarTaskListsItem | None = row.get_next_sibling()
        #         list: TaskList = row.task_list
        #         self.window.stack.remove(list)
        #         if prev_child or next_child:
        #             self.lists.select_row(prev_child or next_child)
        #         self.lists.remove(row)

        # # Update old lists
        # for l in self._get_task_lists():
        #     l.update_ui()

        # # Create new lists
        # old_uids: list[str] = [row.uid for row in self._get_task_lists_items()]
        # new_lists: list[dict] = UserData.get_lists_as_dicts()
        # for l in new_lists:
        #     if l["uid"] not in old_uids:
        #         Log.debug(f"Lists: Add list '{l['uid']}'")
        #         row: SidebarTaskListsItem = self.add_list(l["name"], l["uid"])
        #         self.lists.select_row(row)
        #         self.window.stack.set_visible_child_name(l["name"])
        #         self.sidebar.status_page.set_visible(False)

        # # Show status
        # length: int = len(self._get_task_lists_items())
        # self.sidebar.status_page.set_visible(length == 0)
        # if length == 0:
        #     self.window.stack.set_visible_child_name("status")


class SidebarTodayItem(Gtk.ListBoxRow):
    def __init__(self, window: Window) -> None:
        super().__init__()
        self.window: Window = window
        self.__build_ui()

    def __build_ui(self) -> None:
        self.window.stack.add_titled(
            Adw.StatusPage(title=_("No Tasks for Today")),
            "errands_today_page",
            _("Today"),
        )

        hbox = Gtk.Box(
            margin_start=3,
            spacing=12,
            height_request=50,
        )
        hbox.append(Gtk.Image(icon_name="errands-calendar-symbolic"))
        hbox.append(Gtk.Label(label=_("Today"), hexpand=True, halign=Gtk.Align.START))

        self.set_child(hbox)


class SidebarTrashItem(Gtk.ListBoxRow):
    def __init__(self, window: Window) -> None:
        super().__init__()
        self.window: Window = window
        self.__build_ui()
        self.__create_actions()

    def __create_actions(self) -> None:
        group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
        self.insert_action_group(name="trash_item", group=group)

        def __create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        __create_action("clear", self.trash.on_trash_clear)
        __create_action("restore", self.trash.on_trash_restore)

    def __build_ui(self) -> None:
        self.trash = self.window.trash = Trash(self.window)
        self.window.stack.add_titled(self.trash, "errands_trash_page", _("Trash"))

        hbox = Gtk.Box(
            margin_start=3,
            spacing=12,
            height_request=50,
        )
        hbox.append(Gtk.Image(icon_name="errands-trash-symbolic"))
        hbox.append(Gtk.Label(label=_("Trash"), hexpand=True, halign=Gtk.Align.START))

        # Trash Menu
        trash_menu: Gio.Menu = Gio.Menu.new()
        trash_menu.append(_("Restore"), "trash_item.restore")
        trash_menu.append(_("Clear"), "trash_item.clear")
        hbox.append(
            Gtk.MenuButton(
                menu_model=trash_menu,
                icon_name="view-more-symbolic",
                valign=Gtk.Align.CENTER,
                css_classes=["flat"],
            )
        )

        # Trash drop controller
        trash_drop_ctrl = Gtk.DropTarget.new(actions=Gdk.DragAction.MOVE, type=Task)
        trash_drop_ctrl.connect("drop", lambda _d, task, _x, _y: task.delete())
        self.add_controller(trash_drop_ctrl)

        self.set_child(hbox)


class SidebarTaskListItem(Gtk.ListBoxRow):
    def __init__(self, task_list: TaskList, task_lists: Gtk.ListBox) -> None:
        super().__init__()
        self.task_lists: Gtk.ListBox = task_lists
        self.task_list: TaskList = task_list
        self.uid: str = task_list.list_uid
        self.window: Window = task_list.window
        self._build_ui()
        self._add_actions()

    def _add_actions(self) -> None:
        group: Gio.SimpleActionGroup = Gio.SimpleActionGroup()
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
                next_row: SidebarTaskListItem = self.get_next_sibling()
                prev_row: SidebarTaskListItem = self.get_prev_sibling()
                self.list_box.remove(self)
                if next_row or prev_row:
                    self.list_box.select_row(next_row or prev_row)
                else:
                    self.window.stack.set_visible_child_name("status")
                    self.task_lists.sidebar.status_page.set_visible(True)

                # Remove items from trash
                for item in get_children(self.window.trash.trash_list):
                    if item.task_widget.list_uid == self.uid:
                        self.window.trash.trash_list.remove(item)

                Sync.sync()

            dialog: Adw.MessageDialog = Adw.MessageDialog(
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
                self.task_list.title.set_title(text)
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

    def _build_ui(self) -> None:
        self.add_css_class("task-lists-item")
        self.props.height_request = 50

        # Label
        self.label: Gtk.Label = Gtk.Label(
            halign="start",
            hexpand=True,
            ellipsize=3,
        )
        self.task_list.headerbar.title.bind_property(
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
        ctrl: Gtk.GestureClick = Gtk.GestureClick()
        ctrl.connect("released", self._on_click)
        self.add_controller(ctrl)

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

        self.set_child(
            Box(
                children=[
                    self.label,
                    Gtk.MenuButton(
                        menu_model=menu,
                        icon_name="view-more-symbolic",
                        tooltip_text=_("Menu"),
                        css_classes=["flat"],
                        valign=Gtk.Align.CENTER,
                    ),
                ],
                margin_start=3,
            )
        )

    def _on_click(self, *args) -> None:
        self.window.stack.set_visible_child_name(self.label.get_label())
        self.window.split_view.set_show_content(True)

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
        uid: str = task.uid
        task.purge()
        self.task_list.add_task(uid)
        Sync.sync()

    def update_ui(self):
        pass
