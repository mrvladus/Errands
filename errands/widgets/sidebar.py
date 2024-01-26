# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from re import sub
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.window import Window

from caldav import Todo
from datetime import datetime
from uuid import uuid4
from icalendar import Calendar
from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.widgets.components import Box
from errands.widgets.task import Task
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk, Gio, GObject, Gdk


class Sidebar(Adw.Bin):
    def __init__(self, window: Window):
        super().__init__()
        self.window: Window = window
        self._build_ui()

    def _build_ui(self):
        # Components
        self.status_page: SidebarStatusPage = SidebarStatusPage(self)
        self.task_lists: SidebarTaskLists = SidebarTaskLists(self)
        self.header_bar: SidebarHeaderBar = SidebarHeaderBar(
            self.task_lists, self.window
        )
        self.trash_button: SidebarTrashButton = SidebarTrashButton(
            self.task_lists, self.window
        )
        # Toolbar view
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(
            content=Box(
                children=[
                    self.status_page,
                    self.task_lists,
                    self.trash_button,
                ],
                orientation=Gtk.Orientation.VERTICAL,
            )
        )
        toolbar_view.add_top_bar(self.header_bar)
        self.set_child(toolbar_view)


class SidebarHeaderBar(Adw.Bin):
    def __init__(self, task_lists: SidebarTaskLists, window: Window):
        super().__init__()
        self.task_lists: SidebarTaskLists = task_lists
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
        import_menu.append(_("Import List"), "lists.import")

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
        top_section: Gio.Menu = Gio.Menu.new()
        top_section.append(_("Sync / Fetch Tasks"), "app.sync")
        # Backups
        # backup_submenu: Gio.Menu = Gio.Menu.new()
        # backup_submenu.append(_("Create"), "lists.backup_create")
        # backup_submenu.append(_("Load"), "lists.backup_load")
        # top_section.append_submenu(_("Backup"), backup_submenu)
        menu.append_section(None, top_section)
        bottom_section: Gio.Menu = Gio.Menu.new()
        bottom_section.append(_("Preferences"), "app.preferences")
        bottom_section.append(_("Keyboard Shortcuts"), "win.show-help-overlay")
        bottom_section.append(_("About Errands"), "app.about")
        menu.append_section(None, bottom_section)
        menu_btn: Gtk.MenuButton = Gtk.MenuButton(
            menu_model=menu,
            primary=True,
            icon_name="open-menu-symbolic",
            tooltip_text=_("Main Menu"),
        )
        hb.pack_end(menu_btn)

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
            row = self.task_lists.add_list(name, uid)
            self.task_lists.lists.select_row(row)
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


class SidebarStatusPage(Adw.Bin):
    def __init__(self, sidebar: Sidebar):
        super().__init__()
        self.sidebar: Sidebar = sidebar
        self._build_ui()

    def _build_ui(self) -> None:
        status_page: Adw.StatusPage = Adw.StatusPage(
            title=_("Add new List"),
            description=_('Click "+" button'),
            icon_name="errands-lists-symbolic",
            css_classes=["compact"],
            vexpand=True,
        )
        self.set_child(status_page)


class SidebarTaskLists(Gtk.ScrolledWindow):
    def __init__(self, sidebar: Sidebar):
        super().__init__()
        self.sidebar: Sidebar = sidebar
        self.window: Window = sidebar.window
        self._build_ui()
        self._add_actions()
        self._load_lists()
        self.update_ui()

    def _add_actions(self) -> None:
        group = Gio.SimpleActionGroup()
        self.insert_action_group(name="lists", group=group)

        def _create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        def _add(*args) -> None:
            pass

        def _backup_create(*args) -> None:
            pass

        def _backup_load(*args) -> None:
            pass

        def _import(*args) -> None:
            def _confirm(dialog: Gtk.FileDialog, res) -> None:
                try:
                    file: Gio.File = dialog.open_finish(res)
                except:
                    Log.debug("Lists: Import cancelled")
                    return
                with open(file.get_path(), "r") as f:
                    calendar: Calendar = Calendar.from_ical(f.read())
                    # List name
                    name = calendar.get(
                        "X-WR-CALNAME", file.get_basename().rstrip(".ics")
                    )
                    if name in [
                        i[0]
                        for i in UserData.run_sql("SELECT name FROM lists", fetch=True)
                    ]:
                        name = f"{name}_{uuid4()}"
                    # Create list
                    uid: str = UserData.add_list(name)
                    # Add tasks
                    for todo in calendar.walk("VTODO"):
                        # Tags
                        if (tags := todo.get("CATEGORIES", "")) != "":
                            tags = ",".join(
                                [
                                    i.to_ical().decode("utf-8")
                                    for i in (
                                        tags if isinstance(tags, list) else tags.cats
                                    )
                                ]
                            )
                        # Start
                        if (start := todo.get("DTSTART", "")) != "":
                            start = (
                                todo.get("DTSTART", "")
                                .to_ical()
                                .decode("utf-8")
                                .strip("Z")
                            )
                        else:
                            start = ""
                        # End
                        if (end := todo.get("DUE", todo.get("DTEND", ""))) != "":
                            end = (
                                todo.get("DUE", todo.get("DTEND", ""))
                                .to_ical()
                                .decode("utf-8")
                                .strip("Z")
                            )
                        else:
                            end = ""
                        UserData.add_task(
                            color=todo.get("X-ERRANDS-COLOR", ""),
                            completed=str(todo.get("STATUS", "")) == "COMPLETED",
                            end_date=end,
                            list_uid=uid,
                            notes=str(todo.get("DESCRIPTION", "")),
                            parent=str(todo.get("RELATED-TO", "")),
                            percent_complete=int(todo.get("PERCENT-COMPLETE", 0)),
                            priority=int(todo.get("PRIORITY", 0)),
                            start_date=start,
                            tags=tags,
                            text=str(todo.get("SUMMARY", "")),
                            uid=todo.get("UID", None),
                        )
                self.update_ui()
                self.window.add_toast(_("Imported"))
                Sync.sync()

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(default_filter=filter)
            dialog.open(self.window, None, _confirm)

        _create_action("add", _add)
        _create_action("backup_create", _backup_create)
        _create_action("backup_load", _backup_load)
        _create_action("import", _import)

    def _build_ui(self) -> None:
        self.set_propagate_natural_height(True)
        self.set_vexpand(True)
        # Lists
        self.lists: Gtk.ListBox = Gtk.ListBox(css_classes=["navigation-sidebar"])
        self.lists.connect("row-selected", self._on_row_selected)
        self.bind_property(
            "visible",
            self.sidebar.status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE
            | GObject.BindingFlags.INVERT_BOOLEAN
            | GObject.BindingFlags.BIDIRECTIONAL,
        )
        self.set_child(self.lists)

    def add_list(self, name, uid) -> SidebarTaskListsItem:
        task_list: TaskList = TaskList(self.window, uid, self)
        page: Adw.ViewStackPage = self.window.stack.add_titled(
            child=task_list, name=name, title=name
        )
        task_list.title.bind_property(
            "title", page, "title", GObject.BindingFlags.SYNC_CREATE
        )
        task_list.title.bind_property(
            "title", page, "name", GObject.BindingFlags.SYNC_CREATE
        )
        row: SidebarTaskListsItem = SidebarTaskListsItem(task_list, self)
        self.lists.append(row)
        return row

    def _on_row_selected(self, _, row: Gtk.ListBoxRow) -> None:
        Log.debug("Lists: Switch list")
        if row:
            name = row.label.get_label()
            self.window.stack.set_visible_child_name(name)
            self.window.split_view.set_show_content(True)
            GSettings.set("last-open-list", "s", name)
            self.sidebar.status_page.set_visible(False)

    def _get_task_lists(self) -> list[TaskList]:
        lists: list[TaskList] = []
        pages: Adw.ViewStackPages = self.window.stack.get_pages()
        for i in range(pages.get_n_items()):
            child: TaskList = pages.get_item(i).get_child()
            if isinstance(child, TaskList):
                lists.append(child)
        return lists

    def _get_task_lists_items(self) -> list[SidebarTaskListsItem]:
        return get_children(self.lists)

    def _load_lists(self) -> None:
        lists: list[dict] = [
            i for i in UserData.get_lists_as_dicts() if not i["deleted"]
        ]
        for l in lists:
            row: SidebarTaskListsItem = self.add_list(l["name"], l["uid"])
            if GSettings.get("last-open-list") == l["name"]:
                self.lists.select_row(row)
        self.sidebar.status_page.set_visible(len(lists) == 0)

    def update_ui(self) -> None:
        Log.debug("Lists: Update UI...")

        # Delete lists
        uids: list[str] = [i["uid"] for i in UserData.get_lists_as_dicts()]
        for row in self._get_task_lists_items():
            if row.uid not in uids:
                prev_child: SidebarTaskListsItem | None = row.get_prev_sibling()
                next_child: SidebarTaskListsItem | None = row.get_next_sibling()
                list: TaskList = row.task_list
                self.window.stack.remove(list)
                if prev_child or next_child:
                    self.lists.select_row(prev_child or next_child)
                self.lists.remove(row)

        # Update old lists
        for l in self._get_task_lists():
            l.update_ui()

        # Create new lists
        old_uids: list[str] = [row.uid for row in self._get_task_lists_items()]
        new_lists: list[dict] = UserData.get_lists_as_dicts()
        for l in new_lists:
            if l["uid"] not in old_uids:
                Log.debug(f"Lists: Add list '{l['uid']}'")
                row: SidebarTaskListsItem = self.add_list(l["name"], l["uid"])
                self.lists.select_row(row)
                self.window.stack.set_visible_child_name(l["name"])
                self.sidebar.status_page.set_visible(False)

        # Show status
        length: int = len(self._get_task_lists_items())
        self.sidebar.status_page.set_visible(length == 0)
        if length == 0:
            self.window.stack.set_visible_child_name("status")


class SidebarTaskListsItem(Gtk.ListBoxRow):
    def __init__(self, task_list: TaskList, task_lists: SidebarTaskLists) -> None:
        super().__init__()
        self.task_lists: SidebarTaskLists = task_lists
        self.list_box: Gtk.ListBox = task_lists.lists
        self.task_list: TaskList = task_list
        self.uid: str = task_list.list_uid
        self.window: Window = task_lists.window
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
                next_row: SidebarTaskListsItem = self.get_next_sibling()
                prev_row: SidebarTaskListsItem = self.get_prev_sibling()
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

        # Label
        self.label: Gtk.Label = Gtk.Label(
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
        ctrl: Gtk.GestureClick = Gtk.GestureClick()
        ctrl.connect("released", self._on_click)
        self.add_controller(ctrl)

        # TODO Drop controller
        # drop_ctrl: Gtk.DropTarget = Gtk.DropTarget.new(
        #     actions=Gdk.DragAction.MOVE, type=Task
        # )
        # drop_ctrl.connect("drop", self._on_task_drop)
        # self.add_controller(drop_ctrl)

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

    # TODO
    # def _on_task_drop(self, _drop, task: Task, _x, _y):
    #     return
    # if task.list_uid == self.uid:
    #     return
    # print(task.get_prop("parent"), "=>", self.uid, "=>", task.uid)
    # uid = task.uid
    # task.update_props(["parent", "list_uid"], ["", self.uid])
    # task.purge()
    # self.task_list.add_task(uid)
    # Sync.sync()


class SidebarTrashButton(Gtk.Button):
    def __init__(self, task_lists: SidebarTaskLists, window: Window):
        super().__init__()
        self.task_lists: SidebarTaskLists = task_lists
        self.window: Window = window
        self._build_ui()

    def _build_ui(self):
        self.set_child(
            Adw.ButtonContent(
                icon_name="errands-trash-symbolic",
                label=_("Trash"),
                halign="center",
            )
        )
        self.add_css_class("flat")
        self.set_margin_top(6)
        self.set_margin_bottom(6)
        self.set_margin_start(6)
        self.set_margin_end(6)
        self.task_lists.bind_property(
            "visible",
            self,
            "visible",
            GObject.BindingFlags.SYNC_CREATE,
        )

        # Drop controller
        trash_drop_ctrl = Gtk.DropTarget.new(actions=Gdk.DragAction.MOVE, type=Task)
        trash_drop_ctrl.connect("drop", lambda _d, t, _x, _y: t.delete())
        self.add_controller(trash_drop_ctrl)

    def do_clicked(self) -> None:
        self.task_lists.lists.unselect_all()
        self.window.stack.set_visible_child_name("trash")
        self.window.split_view.set_show_content(True)
